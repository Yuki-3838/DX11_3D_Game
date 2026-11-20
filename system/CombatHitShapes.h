#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <SimpleMath.h>

/**
 * @file CombatHitShapes.h
 * @brief 部位ごとの当たり判定(攻撃する形・攻撃を受ける形)。
 *
 * 以前の判定は、体をまるごと1つの形で扱っていた。
 *  - プレイヤーの剣 → 敵: 剣の箱と、敵のモデル全体を包む箱(OBB)1つ
 *  - 敵 → プレイヤー: 敵の中心からの距離と、正面からの角度(扇形)だけ
 * そのため、前脚を振り下ろす攻撃で懐(爪の内側)に入っていても、扇形の中にいるだけで当たっていた
 * (審査会での指摘)。
 *
 * ここでは体を骨に沿ったカプセル(線分 + 半径)の集まりで表し、部位ごとに
 *  - 受ける形(hurt): 頭・首・胴・尾・脚・翼(敵)、頭・胴・腕・脚(プレイヤー)
 *  - 攻撃する形(attack): その攻撃で振っている部位(爪・頭・尾、突進なら体)
 * を持たせる。骨の位置から毎フレーム作るので、首を伸ばす・尾を振る・前転で体を丸めるといった
 * 姿勢の変化がそのまま判定になる。
 */
namespace Combat
{
/** 判定の部位。デバッグ表示と、どこに当たったかの記録に使う。 */
enum class HitPart : unsigned char
{
    Head,
    Neck,
    Body,
    Tail,
    FrontLegL,
    FrontLegR,
    BackLegL,
    BackLegR,
    WingL,
    WingR,
    ArmL,
    ArmR,
    LegL,
    LegR,
    Claw,
    Count
};

inline const char* HitPartDebugName(HitPart part)
{
    switch (part)
    {
    case HitPart::Head:      return "頭";
    case HitPart::Neck:      return "首";
    case HitPart::Body:      return "胴";
    case HitPart::Tail:      return "尾";
    case HitPart::FrontLegL: return "左前脚";
    case HitPart::FrontLegR: return "右前脚";
    case HitPart::BackLegL:  return "左後脚";
    case HitPart::BackLegR:  return "右後脚";
    case HitPart::WingL:     return "左翼";
    case HitPart::WingR:     return "右翼";
    case HitPart::ArmL:      return "左腕";
    case HitPart::ArmR:      return "右腕";
    case HitPart::LegL:      return "左脚";
    case HitPart::LegR:      return "右脚";
    case HitPart::Claw:      return "爪";
    default:                 return "?";
    }
}

/** 線分 a→b を半径 radius で太らせた形。a==b なら球。 */
struct HitCapsule
{
    DirectX::SimpleMath::Vector3 a{};
    DirectX::SimpleMath::Vector3 b{};
    float radius = 0.0f;
    HitPart part = HitPart::Body;
};

using HitShapeList = std::vector<HitCapsule>;

/**
 * 2本の線分の最も近い点どうしを求める(Real-Time Collision Detection 5.1.9 の方法)。
 * 戻り値は距離の2乗。
 */
inline float ClosestPointsSegmentSegment(
    const DirectX::SimpleMath::Vector3& p1, const DirectX::SimpleMath::Vector3& q1,
    const DirectX::SimpleMath::Vector3& p2, const DirectX::SimpleMath::Vector3& q2,
    DirectX::SimpleMath::Vector3& c1, DirectX::SimpleMath::Vector3& c2)
{
    using DirectX::SimpleMath::Vector3;
    constexpr float EPSILON = 1.0e-6f;
    const Vector3 d1 = q1 - p1;
    const Vector3 d2 = q2 - p2;
    const Vector3 r = p1 - p2;
    const float a = d1.Dot(d1);
    const float e = d2.Dot(d2);
    const float f = d2.Dot(r);
    float s = 0.0f;
    float t = 0.0f;
    if (a <= EPSILON && e <= EPSILON)
    {
        c1 = p1;
        c2 = p2;
        return (c1 - c2).LengthSquared();
    }
    if (a <= EPSILON)
    {
        t = (std::clamp)(f / e, 0.0f, 1.0f);
    }
    else
    {
        const float c = d1.Dot(r);
        if (e <= EPSILON)
        {
            s = (std::clamp)(-c / a, 0.0f, 1.0f);
        }
        else
        {
            const float b = d1.Dot(d2);
            const float denom = a * e - b * b;
            s = denom > EPSILON ? (std::clamp)((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
            t = (b * s + f) / e;
            if (t < 0.0f)
            {
                t = 0.0f;
                s = (std::clamp)(-c / a, 0.0f, 1.0f);
            }
            else if (t > 1.0f)
            {
                t = 1.0f;
                s = (std::clamp)((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
    return (c1 - c2).LengthSquared();
}

/**
 * 2つのカプセルが重なっているか。重なっていれば contact に接触点(2本の芯の最も近い点の間で、
 * 半径の比で分けた位置)を入れる。火花を出す位置に使う。
 */
inline bool CapsulesOverlap(
    const HitCapsule& left,
    const HitCapsule& right,
    DirectX::SimpleMath::Vector3* contact = nullptr)
{
    DirectX::SimpleMath::Vector3 c1{};
    DirectX::SimpleMath::Vector3 c2{};
    const float distanceSquared = ClosestPointsSegmentSegment(left.a, left.b, right.a, right.b, c1, c2);
    const float reach = left.radius + right.radius;
    if (distanceSquared > reach * reach)
        return false;
    if (contact != nullptr)
    {
        const float total = (std::max)(reach, 0.0001f);
        *contact = c1 + (c2 - c1) * (left.radius / total);
    }
    return true;
}

/** 攻撃する形と受ける形の組で、最初に重なったもの。 */
struct HitShapeContact
{
    bool hit = false;
    int attackIndex = -1;
    int hurtIndex = -1;
    DirectX::SimpleMath::Vector3 point{};
};

inline HitShapeContact FindFirstOverlap(const HitShapeList& attack, const HitShapeList& hurt)
{
    HitShapeContact result{};
    for (int i = 0; i < static_cast<int>(attack.size()); ++i)
    {
        for (int j = 0; j < static_cast<int>(hurt.size()); ++j)
        {
            if (CapsulesOverlap(attack[static_cast<size_t>(i)], hurt[static_cast<size_t>(j)], &result.point))
            {
                result.hit = true;
                result.attackIndex = i;
                result.hurtIndex = j;
                return result;
            }
        }
    }
    return result;
}

/** 形の集まりを包む箱(粗い判定とデバッグ表示用)。空なら false。 */
inline bool HitShapesBounds(
    const HitShapeList& shapes,
    DirectX::SimpleMath::Vector3& outMin,
    DirectX::SimpleMath::Vector3& outMax)
{
    using DirectX::SimpleMath::Vector3;
    if (shapes.empty())
        return false;
    outMin = Vector3(1.0e30f, 1.0e30f, 1.0e30f);
    outMax = Vector3(-1.0e30f, -1.0e30f, -1.0e30f);
    for (const HitCapsule& shape : shapes)
    {
        const Vector3 r(shape.radius, shape.radius, shape.radius);
        outMin = Vector3::Min(outMin, Vector3::Min(shape.a, shape.b) - r);
        outMax = Vector3::Max(outMax, Vector3::Max(shape.a, shape.b) + r);
    }
    return true;
}
} // namespace Combat
