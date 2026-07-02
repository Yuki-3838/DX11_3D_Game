#include "GameApp.h"

#include <cassert>

namespace
{
constexpr wchar_t kWindowClassName[] = L"DX11_3D_Game_Window";
constexpr float kTargetFrameSeconds = 1.0f / 60.0f;
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

    m_profiler.SetBudgetMilliseconds(4.5, 8.5, 3.0, 0.6);
    m_browserDebugReporter.Init(L"debug");
    m_prototypeAttack = Combat::CreatePrototypeHeavySlash();
    m_combatDebugState.currentAttackId = m_prototypeAttack.attackId;
    m_combatDebugState.currentPhase = Combat::AttackPhase::Anticipation;
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

    m_profiler.EndFrame(m_frameTimer.GetDeltaSeconds(), fixedUpdateCount);
    UpdateDebugOutputs();
}

void GameApp::FixedUpdate(float fixedDeltaSeconds)
{
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
    }
    else if (m_combatPhaseSeconds < activeEnd)
    {
        m_combatDebugState.currentPhase = Combat::AttackPhase::Active;
        m_combatDebugState.broadPhaseCandidateCount = 1;
        m_combatDebugState.confirmedCollisionCount = 1;
    }
    else if (m_combatPhaseSeconds < recoveryEnd)
    {
        m_combatDebugState.currentPhase = Combat::AttackPhase::Recovery;
        m_combatDebugState.broadPhaseCandidateCount = 0;
        m_combatDebugState.confirmedCollisionCount = 0;
    }
    else if (m_combatPhaseSeconds < cooldownEnd)
    {
        m_combatDebugState.currentPhase = Combat::AttackPhase::Cooldown;
    }
    else
    {
        m_combatPhaseSeconds = 0.0f;
    }
}

void GameApp::Draw()
{
    assert(m_deviceContext);
    ID3D11RenderTargetView* renderTargets[] = {m_renderTargetView.Get()};
    m_deviceContext->OMSetRenderTargets(1, renderTargets, nullptr);
    m_deviceContext->ClearRenderTargetView(m_renderTargetView.Get(), m_clearColor);
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
