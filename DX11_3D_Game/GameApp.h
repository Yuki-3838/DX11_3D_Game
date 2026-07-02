#pragma once

#include "BrowserDebugReporter.h"
#include "CombatDesign.h"
#include "DebugOverlay.h"
#include "FrameTimer.h"
#include "PerformanceProfiler.h"

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

/**
 * @class GameApp
 * @brief DX11/C++一対一アクションの最小アプリケーション基盤。
 *
 * このクラスはPhase 1の実行順序を1か所に集約します。
 *
 * 1. Win32ウィンドウを作る
 * 2. DirectX 11のDevice/Context/SwapChainを作る
 * 3. 固定タイムステップでゲームロジックを更新する
 * 4. 描画とPresentを行う
 * 5. ImGui向けのデバッグ情報とブラウザ用JSONを書き出す
 *
 * 戦闘、AI、VFXはここへ直接詰め込まず、各システムへ分離していく前提です。
 */
class GameApp
{
public:
    GameApp();
    ~GameApp();

    /**
     * @brief アプリケーションを初期化する。
     * @param hInstance Win32アプリケーションインスタンス。
     * @param windowWidth ウィンドウ幅。
     * @param windowHeight ウィンドウ高さ。
     * @return 初期化に成功した場合はtrue。
     */
    bool Init(HINSTANCE hInstance, int windowWidth, int windowHeight);

    /**
     * @brief メッセージループとゲームループを実行する。
     *
     * `Run`は終了メッセージを受け取るまで戻りません。
     * 1フレーム内では「入力/固定更新/描画/デバッグ出力」の順に進みます。
     */
    void Run();

    /** @brief DirectXとウィンドウ関連リソースを解放する。 */
    void Cleanup();

private:
    bool InitWindow(HINSTANCE hInstance);
    bool InitDirectX();
    void ProcessFrame();
    void FixedUpdate(float fixedDeltaSeconds);
    void Draw();
    void Present();
    void UpdateDebugOutputs();

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    HWND m_windowHandle = nullptr;                 ///< ゲーム表示用のWin32ウィンドウ。
    int m_windowWidth = 1280;                      ///< バックバッファ幅。
    int m_windowHeight = 720;                      ///< バックバッファ高さ。
    bool m_isRunning = false;                      ///< メインループ継続フラグ。
    float m_clearColor[4] = {0.05f, 0.07f, 0.10f, 1.0f};

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;                    ///< DX11リソース生成用Device。
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_deviceContext;      ///< 描画コマンド発行用Context。
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;               ///< 表示用SwapChain。
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView; ///< バックバッファの描画先。

    FrameTimer m_frameTimer;                    ///< 固定更新とフレーム時間測定。
    PerformanceProfiler m_profiler;             ///< CPU/GPU予算の見える化用プロファイラ。
    DebugOverlay m_debugOverlay;                 ///< ImGuiに渡すデバッグ表示モデル。
    BrowserDebugReporter m_browserDebugReporter; ///< ブラウザで確認できるJSON/HTML出力。
    Combat::AttackData m_prototypeAttack;         ///< Phase 1で攻撃フレーム設計を確認するサンプル攻撃。
    Combat::CombatDebugState m_combatDebugState;  ///< ImGui/ブラウザへ渡す戦闘デバッグ状態。
    float m_combatPhaseSeconds = 0.0f;            ///< サンプル攻撃のフェーズ進行時間。
};
