#pragma once

#include <chrono>

/**
 * @class FrameTimer
 * @brief 60FPS固定更新を維持するための時間管理クラス。
 *
 * 描画フレームはPC負荷で揺れますが、戦闘・入力・当たり判定は固定間隔で進めると
 * 「攻撃の前兆」「判定の発生」「硬直」が読みやすくなります。
 */
class FrameTimer
{
public:
    /** @brief タイマーを現在時刻で初期化する。 */
    void Reset();

    /** @brief 1描画フレーム分の経過時間を取り込み、固定更新用の蓄積時間を増やす。 */
    void Tick();

    /** @brief 固定更新を1回実行すべきかを返す。 */
    bool ShouldRunFixedUpdate() const;

    /** @brief 固定更新1回分の時間を消費する。 */
    void ConsumeFixedUpdate();

    /** @brief 直近描画フレームの経過秒を返す。 */
    double GetDeltaSeconds() const { return m_deltaSeconds; }

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point m_previousTime = Clock::now();
    double m_deltaSeconds = 0.0;
    double m_accumulatedSeconds = 0.0;
};
