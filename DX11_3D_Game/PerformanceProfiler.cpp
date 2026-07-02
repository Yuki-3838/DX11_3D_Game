#include "PerformanceProfiler.h"

void PerformanceProfiler::SetBudgetMilliseconds(double cpu, double gpu, double systems, double reserve)
{
    m_snapshot.budget.cpuMilliseconds = cpu;
    m_snapshot.budget.gpuMilliseconds = gpu;
    m_snapshot.budget.systemsMilliseconds = systems;
    m_snapshot.budget.reserveMilliseconds = reserve;
}

void PerformanceProfiler::BeginFrame()
{
    m_frameStart = Clock::now();
}

void PerformanceProfiler::EndFrame(double frameDeltaSeconds, int fixedUpdateCount)
{
    const Clock::time_point frameEnd = Clock::now();
    const double measuredMilliseconds = std::chrono::duration<double, std::milli>(frameEnd - m_frameStart).count();

    m_snapshot.frameMilliseconds = measuredMilliseconds;
    m_snapshot.estimatedFps = frameDeltaSeconds > 0.0 ? 1.0 / frameDeltaSeconds : 0.0;
    m_snapshot.fixedUpdateCount = fixedUpdateCount;
}
