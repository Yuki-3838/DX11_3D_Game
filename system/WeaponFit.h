#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "transform.h"

/**
 * @file WeaponFit.h
 * @brief 武器モデルの「握りの位置」「刃の向き」「重心」を、モデルの形から測る。
 *
 * これまで剣の取り付け(位置・角度・大きさ)は、1本の剣に合わせて手で測った定数だった
 * (`CAnimationMesh`の`m_swordRotationDegrees`など)。そのため武器を差し替えると
 * 手からずれてしまい、別の武器を足すたびに人手で合わせ直す必要があった。
 *
 * 片手剣と大剣では、同じ「剣」でも**握りから重心までの距離が違う**。
 * 大剣は刃が長く重心が前に寄るため、境界ボックスの中心を手に合わせると
 * 柄が手から大きくはみ出す。そこで、
 *   - 刃の向き  : 形が一番長い向き
 *   - 握りの位置: 細い側の端(柄)にある、鍔より手前の部分の中心
 *   - 重心      : 頂点の平均(質量が一様だと仮定した近似)
 * をモデルから測り、「握りを手へ、刃を手の向きへ」合わせられるようにする。
 *
 * ここは純粋な計算だけにしてある(描画にもボーンにも触らない)。
 * 測った値が正しいかどうかを、ゲームを動かさずに確かめられるようにするためである。
 */
namespace WeaponFit
{

/** 武器モデルを測った結果。すべてモデルのローカル座標。 */
struct Result
{
    bool valid = false;
    Vector3 gripLocal{};       ///< 握りの位置(手のひらが来る場所)
    Vector3 bladeAxisLocal{};  ///< 握り→切っ先の向き(単位ベクトル)
    Vector3 centroidLocal{};   ///< 重心(頂点の平均)
    Vector3 tipLocal{};        ///< 切っ先の位置
    /// 刃の平らな面が向いている向き(刃に垂直で、一番薄い向き)。
    /// 刃の向きを合わせるだけでは、刃が横を向いたまま持つことがある。
    /// この向きも手本に合わせると、刃の面が正しく立つ。
    Vector3 flatAxisLocal{};
    float length = 0.0f;       ///< 全長(刃の向きの長さ)
    /// 握りから重心までの距離を全長で割った値。0に近いほど手元重心、1に近いほど先重心。
    /// 片手剣は0.3前後、大剣は0.4〜0.5あたりになる。武器の重さの表現に使える。
    float balanceRatio = 0.0f;
};

namespace Detail
{
/// 3つの軸のうち、一番長い向きを返す(剣は細長いので、これが刃の向きになる)。
inline Vector3 LongestAxis(const Vector3& extent)
{
    if (extent.x >= extent.y && extent.x >= extent.z) return Vector3(1.0f, 0.0f, 0.0f);
    if (extent.y >= extent.z)                         return Vector3(0.0f, 1.0f, 0.0f);
    return Vector3(0.0f, 0.0f, 1.0f);
}

/// 刃の向きに垂直な2つの軸のうち、薄い方(刃の平らな面の向き)を返す。
inline Vector3 ThinnestAxisPerpendicularTo(const Vector3& extent, const Vector3& bladeAxis)
{
    const Vector3 axes[3] = {
        Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f) };
    const float sizes[3] = { extent.x, extent.y, extent.z };
    int thinnest = -1;
    for (int i = 0; i < 3; ++i)
    {
        if (std::abs(axes[i].Dot(bladeAxis)) > 0.5f) // 刃の向きそのものは除く
            continue;
        if (thinnest < 0 || sizes[i] < sizes[thinnest])
            thinnest = i;
    }
    return thinnest >= 0 ? axes[thinnest] : Vector3(0.0f, 0.0f, 1.0f);
}

/**
 * @brief 向きfromを向きtoへ重ねる回転を作る(行ベクトル規約: v * M)。
 *
 * 武器の刃の向きを、手本の刃の向きへ合わせるために使う。
 */
inline Matrix4x4 RotationFromTo(const Vector3& from, const Vector3& to)
{
    const float dot = std::clamp(from.Dot(to), -1.0f, 1.0f);
    if (dot > 0.99999f)
        return Matrix4x4::Identity;
    if (dot < -0.99999f)
    {
        // 真逆。どれでもよいので、fromに垂直な軸で180度回す。
        Vector3 axis = from.Cross(Vector3(1.0f, 0.0f, 0.0f));
        if (axis.LengthSquared() < 0.000001f)
            axis = from.Cross(Vector3(0.0f, 1.0f, 0.0f));
        axis.Normalize();
        return Matrix4x4::CreateFromAxisAngle(axis, 3.14159265f);
    }
    Vector3 axis = from.Cross(to);
    axis.Normalize();
    return Matrix4x4::CreateFromAxisAngle(axis, std::acos(dot));
}
} // namespace Detail

/**
 * @brief 頂点の並びから、武器の握り・刃の向き・重心を測る。
 *
 * @param positions 武器モデルの頂点位置(モデルのローカル座標)。
 * @param sliceCount 長さ方向を何枚に切って太さを調べるか。多いほど細かいが、32もあれば十分。
 *
 * @details
 * 手順:
 *  1. 境界ボックスの一番長い向きを刃の向きとする。
 *  2. その向きに沿って薄く切り、各切片の「軸からの距離の最大値」= 太さを求める。
 *  3. 両端の太さを比べ、**細い側を柄**とする(刃より柄の方が細い、という剣の形を使う)。
 *  4. 柄側から見て太さが最初に大きく跳ね上がる所を鍔とみなし、その手前の中心を握りとする。
 *     鍔が見つからない武器(棒など)では、柄側の端から全長の8%の位置を握りとする。
 *  5. 重心は頂点の平均。質量が一様だと仮定した近似だが、
 *     「片手剣か大剣か」を区別するには十分である。
 */
/// 前処理なしで測る本体。`Analyze()`から呼ばれる。
inline Result AnalyzeCore(const std::vector<Vector3>& positions, int sliceCount = 32)
{
    Result result;
    if (positions.size() < 3)
        return result;

    Vector3 minPosition = positions.front();
    Vector3 maxPosition = positions.front();
    Vector3 sum(0.0f, 0.0f, 0.0f);
    for (const Vector3& position : positions)
    {
        minPosition = Vector3::Min(minPosition, position);
        maxPosition = Vector3::Max(maxPosition, position);
        sum += position;
    }
    const Vector3 extent = maxPosition - minPosition;
    const Vector3 axis = Detail::LongestAxis(extent);
    // 刃に垂直な2つの向きのうち、薄い方が「刃の平らな面の向き」。
    const Vector3 flat = Detail::ThinnestAxisPerpendicularTo(extent, axis);
    const float length = std::max({ extent.x, extent.y, extent.z });
    if (length <= 0.0001f)
        return result;

    const Vector3 center = (minPosition + maxPosition) * 0.5f;
    const float axisMin = minPosition.Dot(axis);

    // 長さ方向に切って、切片ごとの太さ(軸からの距離の最大値)を調べる。
    const int slices = std::max(8, sliceCount);
    std::vector<float> thickness(static_cast<size_t>(slices), 0.0f);
    std::vector<Vector3> sliceSum(static_cast<size_t>(slices), Vector3(0.0f, 0.0f, 0.0f));
    std::vector<int> sliceCountPerBin(static_cast<size_t>(slices), 0);
    for (const Vector3& position : positions)
    {
        const float along = (position.Dot(axis) - axisMin) / length; // 0→1
        int index = static_cast<int>(along * static_cast<float>(slices));
        index = std::clamp(index, 0, slices - 1);
        // 軸から離れている距離(軸に沿った成分を抜いた残り)。
        const Vector3 offset = position - center;
        const Vector3 radial = offset - axis * offset.Dot(axis);
        thickness[static_cast<size_t>(index)] =
            std::max(thickness[static_cast<size_t>(index)], radial.Length());
        sliceSum[static_cast<size_t>(index)] += position;
        ++sliceCountPerBin[static_cast<size_t>(index)];
    }

    // 両端の太さを比べ、細い側を柄とする。
    const int edge = std::max(1, slices / 6);
    float headThickness = 0.0f;
    float tailThickness = 0.0f;
    for (int i = 0; i < edge; ++i)
    {
        headThickness += thickness[static_cast<size_t>(i)];
        tailThickness += thickness[static_cast<size_t>(slices - 1 - i)];
    }
    const bool gripAtMinSide = headThickness <= tailThickness;

    // 柄側から順に見て、太さが跳ね上がる所(鍔)を探す。
    const float maxThickness =
        *std::max_element(thickness.begin(), thickness.end());
    const float guardThreshold = maxThickness * 0.55f;
    int guardIndex = -1;
    for (int step = 0; step < slices; ++step)
    {
        const int index = gripAtMinSide ? step : slices - 1 - step;
        if (thickness[static_cast<size_t>(index)] >= guardThreshold)
        {
            guardIndex = step; // 柄側の端から数えた枚数
            break;
        }
    }
    // 鍔が見つからない・すぐ太くなる場合は、全長の8%を柄とみなす。
    const int gripSlices = (guardIndex > 1)
        ? guardIndex
        : std::max(1, static_cast<int>(static_cast<float>(slices) * 0.08f));

    // 柄の範囲の頂点の平均を握りとする。
    Vector3 gripSum(0.0f, 0.0f, 0.0f);
    int gripPoints = 0;
    for (int step = 0; step < gripSlices; ++step)
    {
        const int index = gripAtMinSide ? step : slices - 1 - step;
        gripSum += sliceSum[static_cast<size_t>(index)];
        gripPoints += sliceCountPerBin[static_cast<size_t>(index)];
    }
    if (gripPoints <= 0)
        return result;

    result.valid = true;
    result.gripLocal = gripSum / static_cast<float>(gripPoints);
    result.centroidLocal = sum / static_cast<float>(positions.size());
    result.bladeAxisLocal = gripAtMinSide ? axis : -axis;
    result.flatAxisLocal = flat;
    result.length = length;
    // 切っ先は、握りから刃の向きへ「全長 - 握りまでの距離」進んだ所。
    const float gripAlong = (result.gripLocal - minPosition).Dot(axis);
    const float gripToTip = gripAtMinSide ? (length - gripAlong) : gripAlong;
    result.tipLocal = result.gripLocal + result.bladeAxisLocal * gripToTip;
    const float gripToCentroid =
        (result.centroidLocal - result.gripLocal).Dot(result.bladeAxisLocal);
    result.balanceRatio = std::clamp(gripToCentroid / length, 0.0f, 1.0f);
    return result;
}

/**
 * @brief 武器モデルを「手本の握り方」へ合わせる行列を作る(行ベクトル規約)。
 *
 * 手本(reference)は、今ちゃんと手に収まっている武器を測った結果。
 * 合わせたい武器(weapon)を、
 *   1. 握りを原点へ移す
 *   2. 刃の向きを手本の刃の向きへ合わせる
 *   3. 刃の平らな面の向きを手本へ合わせる(刃が横を向くのを防ぐ)
 *   4. 長さが desiredLength になるよう拡大縮小する
 *   5. 手本の握りの位置へ移す
 * の順で動かす。これで武器ごとに角度と位置を手で測り直す必要がなくなる。
 *
 * @param weapon        合わせたい武器を測った結果(その武器のモデル空間)。
 * @param reference     手本の武器を測った結果(取り付け先のボーンのローカル空間)。
 * @param desiredLength 取り付けたあとの全長(手本と同じ単位)。
 */
inline Matrix4x4 BuildAttachMatrix(
    const Result& weapon, const Result& reference, float desiredLength)
{
    if (!weapon.valid || !reference.valid || weapon.length <= 0.0001f)
        return Matrix4x4::Identity;

    // 2. 刃の向きを合わせる
    const Matrix4x4 alignBlade =
        Detail::RotationFromTo(weapon.bladeAxisLocal, reference.bladeAxisLocal);
    // 3. 刃の面の向きを合わせる(刃の向きを軸にした回り込みを直す)
    Vector3 weaponFlat = Vector3::TransformNormal(weapon.flatAxisLocal, alignBlade);
    const Vector3 bladeAxis = reference.bladeAxisLocal;
    // 刃の向きの成分を抜いて、刃に垂直な面の中だけで比べる。
    Vector3 referenceFlat =
        reference.flatAxisLocal - bladeAxis * reference.flatAxisLocal.Dot(bladeAxis);
    weaponFlat -= bladeAxis * weaponFlat.Dot(bladeAxis);
    Matrix4x4 alignFlat = Matrix4x4::Identity;
    if (weaponFlat.LengthSquared() > 0.000001f && referenceFlat.LengthSquared() > 0.000001f)
    {
        weaponFlat.Normalize();
        referenceFlat.Normalize();
        alignFlat = Detail::RotationFromTo(weaponFlat, referenceFlat);
    }

    const float scale = desiredLength / weapon.length;
    return Matrix4x4::CreateTranslation(-weapon.gripLocal) *
        alignBlade * alignFlat *
        Matrix4x4::CreateScale(scale) *
        Matrix4x4::CreateTranslation(reference.gripLocal);
}

/**
 * @brief 武器モデルを測る。まず「本体のかたまり」だけを取り出してから測る。
 *
 * 武器のモデルには、本体から離れた小さな部品が一緒に入っていることがある
 * (実際、assets/model/Sword.fbx には剣本体679頂点のほかに、離れた所へ58頂点の部品がある)。
 * そのまま測ると、全長も重心も部品に引っ張られ、取り付けたときに部品が宙に浮いて見える。
 *
 * そこで長さ方向に切って、頂点が続いている一番大きなかたまりだけを本体とみなし、
 * それ以外を捨ててから測る。武器を差し替えても同じ手順で扱えるようにするためである。
 */
inline Result Analyze(const std::vector<Vector3>& positions, int sliceCount = 32)
{
    if (positions.size() < 3)
        return Result{};

    Vector3 minPosition = positions.front();
    Vector3 maxPosition = positions.front();
    for (const Vector3& position : positions)
    {
        minPosition = Vector3::Min(minPosition, position);
        maxPosition = Vector3::Max(maxPosition, position);
    }
    const Vector3 extent = maxPosition - minPosition;
    const Vector3 axis = Detail::LongestAxis(extent);
    const float length = std::max({ extent.x, extent.y, extent.z });
    if (length <= 0.0001f)
        return Result{};

    // 長さ方向に切って、切片ごとの頂点数を数える。
    const int slices = std::max(8, sliceCount);
    std::vector<int> counts(static_cast<size_t>(slices), 0);
    const float axisMin = minPosition.Dot(axis);
    for (const Vector3& position : positions)
    {
        int index = static_cast<int>(((position.Dot(axis) - axisMin) / length) *
            static_cast<float>(slices));
        index = std::clamp(index, 0, slices - 1);
        ++counts[static_cast<size_t>(index)];
    }

    // 頂点がほとんど無い切片は「隙間」とみなし、続いているかたまりを探す。
    int maxCount = 0;
    for (int count : counts)
        maxCount = std::max(maxCount, count);
    const int emptyThreshold = std::max(1, maxCount / 20); // 一番多い切片の5%未満は隙間
    int bestStart = 0, bestEnd = slices - 1, bestTotal = -1;
    int runStart = -1, runTotal = 0;
    for (int index = 0; index <= slices; ++index)
    {
        const bool filled = index < slices && counts[static_cast<size_t>(index)] >= emptyThreshold;
        if (filled)
        {
            if (runStart < 0) { runStart = index; runTotal = 0; }
            runTotal += counts[static_cast<size_t>(index)];
            continue;
        }
        if (runStart >= 0 && runTotal > bestTotal)
        {
            bestTotal = runTotal;
            bestStart = runStart;
            bestEnd = index - 1;
        }
        runStart = -1;
    }

    // 一番大きなかたまりが全体なら、そのまま測る。
    if (bestStart == 0 && bestEnd == slices - 1)
        return AnalyzeCore(positions, sliceCount);

    // 本体の範囲の頂点だけを取り出して測り直す。
    const float sliceLength = length / static_cast<float>(slices);
    const float lower = axisMin + static_cast<float>(bestStart) * sliceLength;
    const float upper = axisMin + static_cast<float>(bestEnd + 1) * sliceLength;
    std::vector<Vector3> main;
    main.reserve(positions.size());
    for (const Vector3& position : positions)
    {
        const float along = position.Dot(axis);
        if (along >= lower - 0.001f && along <= upper + 0.001f)
            main.push_back(position);
    }
    if (main.size() < 3)
        return AnalyzeCore(positions, sliceCount);
    return AnalyzeCore(main, sliceCount);
}

} // namespace WeaponFit
