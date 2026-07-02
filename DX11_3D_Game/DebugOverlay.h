#pragma once

#include "PerformanceProfiler.h"

/**
 * @class DebugOverlay
 * @brief Dear ImGuiに表示するデバッグ情報を保持する薄い層。
 *
 * Dear ImGui本体は外部ライブラリとして後から接続できるように、ゲーム側は
 * `PerformanceSnapshot`などの表示モデルだけをここへ渡します。
 */
class DebugOverlay
{
public:
    /** @brief ImGuiウィンドウに表示する最新フレーム情報を更新する。 */
    void SetPerformanceSnapshot(const PerformanceSnapshot& snapshot) { m_performanceSnapshot = snapshot; }

    /** @brief ブラウザ出力やテストから同じデバッグ値を参照する。 */
    const PerformanceSnapshot& GetPerformanceSnapshot() const { return m_performanceSnapshot; }

    /**
     * @brief Dear ImGui描画関数を接続するための拡張点。
     *
     * ImGui導入後はここで `ImGui::Begin` / `ImGui::Text` / `ImGui::End` を呼びます。
     * 現時点では依存ライブラリを増やさず、表示データの受け皿だけを固定しています。
     */
    void BuildImGuiFrame();

private:
    PerformanceSnapshot m_performanceSnapshot;
};
