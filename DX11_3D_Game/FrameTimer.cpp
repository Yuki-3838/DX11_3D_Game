#include "FrameTimer.h"

namespace
{
constexpr double kFixedStepSeconds = 1.0 / 60.0;
constexpr double kMaxFrameSeconds = 0.25;
}

void FrameTimer::Reset()
{
    m_previousTime = Clock::now();
    m_deltaSeconds = 0.0;
    m_accumulatedSeconds = 0.0;
}

void FrameTimer::Tick()
{
    const Clock::time_point currentTime = Clock::now();
    m_deltaSeconds = std::chrono::duration<double>(currentTime - m_previousTime).count();
    m_previousTime = currentTime;

    if (m_deltaSeconds > kMaxFrameSeconds)
    {
        m_deltaSeconds = kMaxFrameSeconds;
    }

    m_accumulatedSeconds += m_deltaSeconds;
}

bool FrameTimer::ShouldRunFixedUpdate() const
{
    return m_accumulatedSeconds >= kFixedStepSeconds;
}

void FrameTimer::ConsumeFixedUpdate()
{
    m_accumulatedSeconds -= kFixedStepSeconds;
}
