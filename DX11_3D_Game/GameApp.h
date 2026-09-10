#pragma once

#include "BrowserDebugReporter.h"
#include "CombatDesign.h"
#include "CombatCollision.h"
#include "DebugOverlay.h"
#include "FrameTimer.h"
#include "ImGuiDebugAdapter.h"
#include "PerformanceProfiler.h"
#include "SimpleDebugRenderer.h"

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

/**
 * @class GameApp
 * @brief DX11一対一アクション試作のアプリケーション本体。
 *
 * ウィンドウループ、DX11デバイス、固定更新の戦闘処理、デバッグ出力を管理する。
 * ゲーム処理はWin32ループへ直接書かず、別クラスとしてFixedUpdateから呼び出す。
 */
class GameApp
{
public:
    GameApp();
    ~GameApp();

    /**
     * @brief ウィンドウ、DX11オブジェクト、戦闘データ、デバッグ出力を初期化する。
     */
    bool Init(HINSTANCE hInstance, int windowWidth, int windowHeight);

    /**
     * @brief Win32メッセージループと固定更新のゲームループを実行する。
     */
    void Run();

    /** @brief DX11とウィンドウのリソースを解放する。 */
    void Cleanup();

private:
    bool InitWindow(HINSTANCE hInstance);
    bool InitDirectX();
    void ProcessFrame();
    void FixedUpdate(float fixedDeltaSeconds);
    void Draw();
    void DrawCombatPrototype();
    void BeginImGuiFrame();
    void RenderImGui();
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
    ImGuiDebugAdapter m_imguiDebugAdapter;
    bool m_imguiInitialized = false;

    Combat::AttackData m_prototypeAttack;
    Combat::CombatDebugState m_combatDebugState;
    float m_combatPhaseSeconds = 0.0f;
    float m_playerX = -0.45f;
    float m_enemyX = 0.45f;
    Combat::AttackCollisionResult m_collisionResult;
    bool m_attackHitRegistered = false;
};
