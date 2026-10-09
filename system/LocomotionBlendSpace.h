#pragma once

#include <algorithm>
#include <array>
#include <cmath>

/**
 * @file LocomotionBlendSpace.h
 * @brief 移動アニメーションの2Dブレンドツリー(ブレンドスペース)の重み計算。
 *
 * 以前の移動アニメーションは「歩き」「走り」の2本を速度のしきい値で即座に切り替えていた。
 * そのため歩き出し・走り出し・止まる瞬間に足の姿勢が飛び、
 * ロックオン中に横や後ろへ動いても前向きに歩くクリップしか再生できなかった。
 *
 * ここではキャラクターから見た移動速度(右方向・前方向)から、
 * 待機と8方向(前後左右 × 歩き/走り)のクリップの重みを求める。
 * 重みは速度に対して連続的に変わるので、切り替わりの瞬間が存在しない。
 *
 *   方向の重み : 前・右・後・左のうち、移動方向を挟む隣り合う2方向を角度で按分する。
 *   歩調の重み : 速さが歩きの速さ以下なら「待機↔歩き」、それを超えたら「歩き↔走り」で按分する。
 *   最終的な重み = 方向の重み × 歩調の重み(+待機)。同時に使うクリップは最大5本。
 *
 * 計算は純粋な関数にしてあり、描画やボーンに依存しない。
 */
namespace Anim
{
/** 移動方向。前から時計回り(上から見て)に並べている。 */
enum class LocomotionDirection : int
{
    Forward = 0,
    Right = 1,
    Backward = 2,
    Left = 3,
    Count = 4,
};

/** 歩調。 */
enum class LocomotionGait : int
{
    Walk = 0,
    Run = 1,
    Count = 2,
};

/** 1本のクリップの重み。 */
struct LocomotionClipWeight
{
    LocomotionDirection direction = LocomotionDirection::Forward;
    LocomotionGait gait = LocomotionGait::Walk;
    float weight = 0.0f;
};

/** ブレンドスペースの評価結果。 */
struct LocomotionWeights
{
    float idle = 1.0f;                           ///< 待機クリップの重み
    std::array<LocomotionClipWeight, 4> moving{}; ///< 移動クリップの重み(最大4本)
    int movingCount = 0;
    float speed = 0.0f;                          ///< 評価に使った速さ
};

/**
 * @brief キャラクターから見た移動速度から、各クリップの重みを求める。
 *
 * @param velocityRight   右方向の速度(単位/秒)。負なら左。
 * @param velocityForward 前方向の速度(単位/秒)。負なら後ろ。
 * @param walkSpeed       歩きクリップが最も合う速さ。
 * @param runSpeed        走りクリップが最も合う速さ。
 */
inline LocomotionWeights ComputeLocomotionWeights(
    float velocityRight, float velocityForward, float walkSpeed, float runSpeed)
{
    LocomotionWeights result{};
    const float speed = std::sqrt(velocityRight * velocityRight + velocityForward * velocityForward);
    result.speed = speed;

    // ほぼ止まっているときは待機だけにする。方向が決まらず、ごく小さな速度で
    // 重みが暴れるのを避けるため。
    constexpr float STOP_EPSILON = 0.5f;
    if (speed < STOP_EPSILON || walkSpeed <= 0.0f)
        return result;

    // --- 歩調の重み ---
    float idleWeight = 0.0f;
    float walkWeight = 0.0f;
    float runWeight = 0.0f;
    if (speed <= walkSpeed || runSpeed <= walkSpeed)
    {
        walkWeight = std::clamp(speed / walkSpeed, 0.0f, 1.0f);
        idleWeight = 1.0f - walkWeight;
    }
    else
    {
        runWeight = std::clamp((speed - walkSpeed) / (runSpeed - walkSpeed), 0.0f, 1.0f);
        walkWeight = 1.0f - runWeight;
    }

    // --- 方向の重み ---
    // 前を0度、右を90度とする角度(時計回り)。
    constexpr float PI_F = 3.14159265f;
    float angle = std::atan2(velocityRight, velocityForward);
    if (angle < 0.0f)
        angle += 2.0f * PI_F;
    const float sector = angle / (0.5f * PI_F);
    const int first = static_cast<int>(std::floor(sector)) % 4;
    const int second = (first + 1) % 4;
    const float secondWeight = sector - std::floor(sector);
    const float firstWeight = 1.0f - secondWeight;

    result.idle = idleWeight;
    const auto push = [&result](int direction, LocomotionGait gait, float weight)
    {
        if (weight <= 0.001f || result.movingCount >= static_cast<int>(result.moving.size()))
            return;
        result.moving[result.movingCount++] = {
            static_cast<LocomotionDirection>(direction), gait, weight };
    };
    push(first, LocomotionGait::Walk, firstWeight * walkWeight);
    push(second, LocomotionGait::Walk, secondWeight * walkWeight);
    push(first, LocomotionGait::Run, firstWeight * runWeight);
    push(second, LocomotionGait::Run, secondWeight * runWeight);

    // 閾値未満で落とした重みの分を正規化し直す(合計を1に保つ)。
    float total = result.idle;
    for (int i = 0; i < result.movingCount; ++i)
        total += result.moving[i].weight;
    if (total > 0.0001f)
    {
        result.idle /= total;
        for (int i = 0; i < result.movingCount; ++i)
            result.moving[i].weight /= total;
    }
    return result;
}
} // namespace Anim
