#include "GameApp.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <thread>

namespace
{
constexpr wchar_t kWindowClassName[] = L"DX11_3D_Game_Window";
constexpr float kTargetFrameSeconds = 1.0f / 60.0f;
constexpr float kPrototypeMinX = -0.82f;
constexpr float kPrototypeMaxX = 0.82f;

using FrameClock = std::chrono::steady_clock;

double WaitForLockedFrameRate(const FrameClock::time_point& frameStart)
{
    constexpr auto targetFrameDuration = std::chrono::duration<double>(kTargetFrameSeconds);
    const FrameClock::time_point targetEnd = frameStart + std::chrono::duration_cast<FrameClock::duration>(targetFrameDuration);
    const FrameClock::time_point sleepEnd = targetEnd - std::chrono::milliseconds(1);
    const FrameClock::time_point beforeSleep = FrameClock::now();

    if (beforeSleep < sleepEnd)
    {
        std::this_thread::sleep_until(sleepEnd);
    }

    while (FrameClock::now() < targetEnd)
    {
        std::this_thread::yield();
    }

    const FrameClock::time_point frameEnd = FrameClock::now();
    return std::chrono::duration<double>(frameEnd - frameStart).count();
}
}

GameApp::GameApp() = default;

GameApp::~GameApp()
{
    Cleanup();
}

bool GameApp::Init(HINSTANCE hInstance, int windowWidth, int windowHeight)
{
    m_windowWidth = windowWidth;
    m_windowHeight = windowHeight;

    if (!InitWindow(hInstance))
    {
        return false;
    }

    if (!InitDirectX())
    {
        return false;
    }

    if (!m_simpleDebugRenderer.Init(m_device.Get()))
    {
        return false;
    }

    m_profiler.SetBudgetMilliseconds(4.5, 8.5, 3.0, 0.6);
    m_browserDebugReporter.Init(L"debug");
    m_prototypeAttack = Combat::CreatePrototypeHeavySlash();
    m_combatDebugState.currentAttackId = m_prototypeAttack.attackId;
    m_combatDebugState.currentPhase = Combat::AttackPhase::Anticipation;
    m_combatDebugState.playerHp = 100;
    m_combatDebugState.playerStamina = 100;
    m_combatDebugState.enemyHp = 160;
    m_frameTimer.Reset();
    m_isRunning = true;
    return true;
}

void GameApp::Run()
{
    MSG message = {};

    while (m_isRunning)
    {
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                m_isRunning = false;
                break;
            }

            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        if (m_isRunning)
        {
            ProcessFrame();
        }
    }
}

void GameApp::Cleanup()
{
    if (m_deviceContext)
    {
        m_deviceContext->ClearState();
    }

    m_renderTargetView.Reset();
    m_swapChain.Reset();
    m_deviceContext.Reset();
    m_device.Reset();

    if (m_windowHandle)
    {
        DestroyWindow(m_windowHandle);
        m_windowHandle = nullptr;
    }
}

bool GameApp::InitWindow(HINSTANCE hInstance)
{
    WNDCLASSEX windowClass = {};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = GameApp::WndProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWindowClassName;

    if (!RegisterClassEx(&windowClass))
    {
        return false;
    }

    RECT windowRect = {0, 0, m_windowWidth, m_windowHeight};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    m_windowHandle = CreateWindowEx(
        0,
        kWindowClassName,
        L"DX11 3D Game - Phase 1 Prototype",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!m_windowHandle)
    {
        return false;
    }

    SetWindowLongPtr(m_windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    ShowWindow(m_windowHandle, SW_SHOW);
    UpdateWindow(m_windowHandle);
    return true;
}

bool GameApp::InitDirectX()
{
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = static_cast<UINT>(m_windowWidth);
    swapChainDesc.BufferDesc.Height = static_cast<UINT>(m_windowHeight);
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = m_windowHandle;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#if defined(_DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL requestedFeatureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    D3D_FEATURE_LEVEL createdFeatureLevel = D3D_FEATURE_LEVEL_11_0;

    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        requestedFeatureLevels,
        _countof(requestedFeatureLevels),
        D3D11_SDK_VERSION,
        &swapChainDesc,
        m_swapChain.GetAddressOf(),
        m_device.GetAddressOf(),
        &createdFeatureLevel,
        m_deviceContext.GetAddressOf());

    if (FAILED(result))
    {
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    result = m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (FAILED(result))
    {
        return false;
    }

    result = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.GetAddressOf());
    if (FAILED(result))
    {
        return false;
    }

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_windowWidth);
    viewport.Height = static_cast<float>(m_windowHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_deviceContext->RSSetViewports(1, &viewport);

    return true;
}

void GameApp::ProcessFrame()
{
    const FrameClock::time_point frameStart = FrameClock::now();

    m_frameTimer.Tick();
    m_profiler.BeginFrame();

    int fixedUpdateCount = 0;
    while (m_frameTimer.ShouldRunFixedUpdate())
    {
        FixedUpdate(kTargetFrameSeconds);
        m_frameTimer.ConsumeFixedUpdate();
        ++fixedUpdateCount;
    }

    Draw();
    Present();

    const double lockedFrameSeconds = WaitForLockedFrameRate(frameStart);
    m_profiler.EndFrame(lockedFrameSeconds, fixedUpdateCount);
    UpdateDebugOutputs();
}

void GameApp::FixedUpdate(float fixedDeltaSeconds)
{
    const bool moveLeft = (GetAsyncKeyState('A') & 0x8000) != 0;
    const bool moveRight = (GetAsyncKeyState('D') & 0x8000) != 0;
    const bool guard = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool restartAttack = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

    const float moveSpeed = guard ? 0.35f : 0.65f;
    if (moveLeft)
    {
        m_playerX -= moveSpeed * fixedDeltaSeconds;
    }
    if (moveRight)
    {
        m_playerX += moveSpeed * fixedDeltaSeconds;
    }
    m_playerX = std::clamp(m_playerX, kPrototypeMinX, kPrototypeMaxX);

    if (restartAttack)
    {
        m_combatPhaseSeconds = 0.0f;
    }

    m_combatPhaseSeconds += fixedDeltaSeconds;

    const auto& frames = m_prototypeAttack.frames;
    const float anticipationEnd = frames.anticipationSeconds;
    const float activeEnd = anticipationEnd + frames.activeSeconds;
    const float recoveryEnd = activeEnd + frames.recoverySeconds;
    const float cooldownEnd = recoveryEnd + frames.cooldownSeconds;

    if (m_combatPhaseSeconds < anticipationEnd)
    {
        m_combatDebugState.currentPhase = Combat::AttackPhase::Anticipation;
        m_combatDebugState.broadPhaseCandidateCount = 0;
        m_combatDebugState.confirmedCollisionCount = 0;
        m_combatDebugState.enemyInRecovery = false;
    }
    else if (m_combatPhaseSeconds < activeEnd)
    {
        m_combatDebugState.currentPhase = Combat::AttackPhase::Active;
        const float distance = std::abs(m_enemyX - m_playerX);
        const bool inRange = distance <= 0.36f;
        m_combatDebugState.broadPhaseCandidateCount = inRange ? 1 : 0;
        m_combatDebugState.confirmedCollisionCount = inRange && !guard ? 1 : 0;
        m_combatDebugState.enemyInRecovery = false;
    }
    else if (m_combatPhaseSeconds < recoveryEnd)
    {
        m_combatDebugState.currentPhase = Combat::AttackPhase::Recovery;
        m_combatDebugState.broadPhaseCandidateCount = 0;
        m_combatDebugState.confirmedCollisionCount = 0;
        m_combatDebugState.enemyInRecovery = true;
    }
    else if (m_combatPhaseSeconds < cooldownEnd)
    {
        m_combatDebugState.currentPhase = Combat::AttackPhase::Cooldown;
        m_combatDebugState.enemyInRecovery = false;
    }
    else
    {
        m_combatPhaseSeconds = 0.0f;
    }

    m_combatDebugState.playerGuarding = guard;
    m_combatDebugState.distanceMeters = std::abs(m_enemyX - m_playerX) * 4.0f;
    m_combatDebugState.playerStamina = guard ? 82 : 100;
}

void GameApp::Draw()
{
    assert(m_deviceContext);
    ID3D11RenderTargetView* renderTargets[] = {m_renderTargetView.Get()};
    m_deviceContext->OMSetRenderTargets(1, renderTargets, nullptr);
    m_deviceContext->ClearRenderTargetView(m_renderTargetView.Get(), m_clearColor);
    DrawCombatPrototype();
}

void GameApp::DrawCombatPrototype()
{
    const DebugColor arenaColor = {0.12f, 0.15f, 0.18f, 1.0f};
    const DebugColor playerColor = m_combatDebugState.playerGuarding
        ? DebugColor{0.15f, 0.85f, 0.95f, 1.0f}
        : DebugColor{0.25f, 0.55f, 1.0f, 1.0f};
    const DebugColor enemyColor = m_combatDebugState.enemyInRecovery
        ? DebugColor{1.0f, 0.72f, 0.22f, 1.0f}
        : DebugColor{1.0f, 0.22f, 0.18f, 1.0f};
    const DebugColor hitboxColor = {1.0f, 0.25f, 0.08f, 0.80f};
    const DebugColor rangeColor = {0.35f, 0.24f, 0.12f, 1.0f};

    m_simpleDebugRenderer.DrawRect(m_deviceContext.Get(), {-0.90f, 0.12f, 0.90f, -0.55f, arenaColor});
    m_simpleDebugRenderer.DrawRect(m_deviceContext.Get(), {m_playerX - 0.06f, -0.02f, m_playerX + 0.06f, -0.36f, playerColor});
    m_simpleDebugRenderer.DrawRect(m_deviceContext.Get(), {m_enemyX - 0.08f, 0.02f, m_enemyX + 0.08f, -0.36f, enemyColor});

    if (m_combatDebugState.currentPhase == Combat::AttackPhase::Anticipation)
    {
        m_simpleDebugRenderer.DrawRect(m_deviceContext.Get(), {m_enemyX - 0.38f, -0.08f, m_enemyX - 0.08f, -0.30f, rangeColor});
    }
    else if (m_combatDebugState.currentPhase == Combat::AttackPhase::Active)
    {
        m_simpleDebugRenderer.DrawRect(m_deviceContext.Get(), {m_enemyX - 0.40f, -0.06f, m_enemyX - 0.08f, -0.32f, hitboxColor});
    }

    const float phaseProgress = std::clamp(m_combatPhaseSeconds / 2.0f, 0.0f, 1.0f);
    m_simpleDebugRenderer.DrawRect(m_deviceContext.Get(), {-0.90f, 0.76f, 0.90f, 0.70f, {0.18f, 0.20f, 0.24f, 1.0f}});
    m_simpleDebugRenderer.DrawRect(m_deviceContext.Get(), {-0.90f, 0.76f, -0.90f + 1.80f * phaseProgress, 0.70f, {0.35f, 0.85f, 0.48f, 1.0f}});

    const float staminaRatio = static_cast<float>(m_combatDebugState.playerStamina) / 100.0f;
    m_simpleDebugRenderer.DrawRect(m_deviceContext.Get(), {-0.90f, 0.64f, 0.10f, 0.59f, {0.18f, 0.20f, 0.24f, 1.0f}});
    m_simpleDebugRenderer.DrawRect(m_deviceContext.Get(), {-0.90f, 0.64f, -0.90f + staminaRatio, 0.59f, {0.20f, 0.68f, 1.0f, 1.0f}});
}

void GameApp::Present()
{
    if (m_swapChain)
    {
        m_swapChain->Present(1, 0);
    }
}

void GameApp::UpdateDebugOutputs()
{
    const PerformanceSnapshot snapshot = m_profiler.GetSnapshot();
    m_debugOverlay.SetPerformanceSnapshot(snapshot);
    m_debugOverlay.SetCombatDebugState(m_combatDebugState);
    m_browserDebugReporter.Write(snapshot, m_combatDebugState);
}

LRESULT CALLBACK GameApp::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
            return 0;
        }
        break;
    default:
        break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}
