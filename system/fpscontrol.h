#pragma once
#include <chrono>
#include <thread>
#include <cstdint>
#include <stdexcept>

/**
 * @class FPS
 * @brief 指定したFPSでゲームループを動かすためのクラス
 */
class FPS {
public:
    FPS() = delete;

    /**
     * @brief FPSを指定して初期化
     * @param fps 目標FPS
     */
    explicit FPS(uint64_t fps)
        : m_MicrosecondsPerFrame(1000000 / fps)
    {
        if (fps == 0) {
            throw std::invalid_argument("fps must not be zero");
        }

        auto now = std::chrono::steady_clock::now();

        m_frameStartTime = now;
        m_prevFrameStartTime = now;
    }

    /**
     * @brief フレーム開始時に呼ぶ
     * @return 前フレーム開始から今回フレーム開始までの経過時間 マイクロ秒
     */
    uint64_t BeginFrame() {
        auto now = std::chrono::steady_clock::now();

        auto delta_us =
            std::chrono::duration_cast<std::chrono::microseconds>(
                now - m_prevFrameStartTime
            ).count();

        m_prevFrameStartTime = now;
        m_frameStartTime = now;

        return static_cast<uint64_t>(delta_us);
    }

    /**
     * @brief フレーム終了時に呼ぶ
     *
     * 処理が早く終わった場合、目標FPSになるまで待機する。
     */
    void EndFrame() const {
        auto targetTime =
            m_frameStartTime +
            std::chrono::microseconds(m_MicrosecondsPerFrame);

        std::this_thread::sleep_until(targetTime);
    }

private:
    uint64_t m_MicrosecondsPerFrame = 0;

    std::chrono::steady_clock::time_point m_frameStartTime;
    std::chrono::steady_clock::time_point m_prevFrameStartTime;
};