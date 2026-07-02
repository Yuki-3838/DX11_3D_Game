#pragma once

#include "BrowserDebugReporter.h"
#include "CombatDesign.h"
#include "DebugOverlay.h"
#include "FrameTimer.h"
#include "PerformanceProfiler.h"
#include "SimpleDebugRenderer.h"

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

/**
 * @class GameApp
 * @brief Application root for the DX11 one-on-one action prototype.
 *
 * The app owns the platform loop, the DX11 device, fixed-step combat updates,
 * and debug output. Gameplay systems should be added as separate classes and
 * called from `FixedUpdate` instead of being buried directly in the Win32 loop.
 */
class GameApp
{
public:
    GameApp();
    ~GameApp();

    /**
     * @brief Creates the window, DX11 objects, prototype combat data, and debug outputs.
     */
    bool Init(HINSTANCE hInstance, int windowWidth, int windowHeight);

    /**
     * @brief Runs the Win32 message loop and the fixed-step game loop.
     */
    void Run();

    /** @brief Releases DX11 and window resources. */
    void Cleanup();

private:
    bool InitWindow(HINSTANCE hInstance);
    bool InitDirectX();
    void ProcessFrame();
    void FixedUpdate(float fixedDeltaSeconds);
    void Draw();
    void DrawCombatPrototype();
    void Present();
    void UpdateDebugOutputs();

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    HWND m_windowHandle = nullptr;
    int m_windowWidth = 1280;
    int m_windowHeight = 720;
    bool m_isRunning = false;
    float m_clearColor[4] = {0.05f, 0.07f, 0.10f, 1.0f};

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_deviceContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;

    SimpleDebugRenderer m_simpleDebugRenderer;
    FrameTimer m_frameTimer;
    PerformanceProfiler m_profiler;
    DebugOverlay m_debugOverlay;
    BrowserDebugReporter m_browserDebugReporter;

    Combat::AttackData m_prototypeAttack;
    Combat::CombatDebugState m_combatDebugState;
    float m_combatPhaseSeconds = 0.0f;
    float m_playerX = -0.45f;
    float m_enemyX = 0.45f;
};
