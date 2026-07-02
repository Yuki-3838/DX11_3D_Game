#pragma once

class DebugOverlay;

/**
 * @class ImGuiDebugAdapter
 * @brief DebugOverlayの値をDear ImGuiへ描画する接続層。
 *
 * ゲーム本体はDebugOverlayへ値を渡すだけにして、Dear ImGuiへの依存はこのクラスへ閉じ込めます。
 * `DX11_GAME_ENABLE_IMGUI` を定義し、imgui.hをインクルードパスに追加すると実描画が有効になります。
 */
class ImGuiDebugAdapter
{
public:
    /** @brief パフォーマンスと戦闘状態のデバッグウィンドウを構築する。 */
    void Render(const DebugOverlay& overlay);
};
