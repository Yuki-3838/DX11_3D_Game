#pragma once

#include <chrono>

/**
 * @struct PerformanceBudget
 * @brief 60FPSを守るための1フレーム内の時間予算。
 */
struct PerformanceBudget
{
    double cpuMilliseconds = 4.5;        ///< CPU側のゲーム処理予算。
    double gpuMilliseconds = 8.5;        ///< GPU側の描画予算。Phase 1では測定準備値。
    double systemsMilliseconds = 3.0;    ///< 物理、AI、UI、I/Oなどの予算。
    double reserveMilliseconds = 0.6;    ///< 予備時間。
};

/**
 * @struct PerformanceSnapshot
 * @brief デバッグUIやブラウザ出力に渡すフレーム計測値。
 */
struct PerformanceSnapshot
{
    double frameMilliseconds = 0.0;      ///< 実測フレーム時間。
    double estimatedFps = 0.0;           ///< 実測FPS。
    int fixedUpdateCount = 0;            ///< この描画フレーム内で実行した固定更新回数。
    PerformanceBudget budget;            ///< 現在の目標予算。
};

/**
 * @class PerformanceProfiler
 * @brief Phase 1からCPU/GPUどちらが重いかを判断するための計測入口。
 *
 * 現時点ではCPU側のフレーム時間を記録します。DX11のGPU timestamp queryは
 * Renderer側を拡張するときにこのクラスへ接続します。
 */
class PerformanceProfiler
{
public:
    void SetBudgetMilliseconds(double cpu, double gpu, double systems, double reserve);
    void BeginFrame();
    void EndFrame(double frameDeltaSeconds, int fixedUpdateCount);
    PerformanceSnapshot GetSnapshot() const { return m_snapshot; }

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point m_frameStart = Clock::now();
    PerformanceSnapshot m_snapshot;
};
