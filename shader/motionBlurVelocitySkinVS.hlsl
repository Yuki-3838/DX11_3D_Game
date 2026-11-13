#include "common.hlsl"
#include "skinTransform.hlsli"
#include "motionBlurCommon.hlsli"

// キャラクターの本体を、今のフレームと前のフレームの姿勢で変換する。
// 本描画と同じスキニングを使わないと、速度の輪郭が見た目の輪郭からずれる。

VELOCITY_PS_IN main(in VSONESKIN_IN In)
{
    float4x4 previousBones[4];
    for (int i = 0; i < 4; ++i)
    {
        previousBones[i] = (float4x4) 0;
        if (In.BoneIndex[i] >= 0 && In.BoneIndex[i] < MAX_BONE)
            previousBones[i] = PreviousBoneMatrix[In.BoneIndex[i]];
    }

    const float4 currentPosition = SkinPositionHybrid(In);
    const float4 previousPosition =
        SkinVertexFromBones(In, float3(0.0f, 1.0f, 0.0f), previousBones).position;

    // 深度を本描画とそろえるため、行列を掛ける順番も本描画と同じにする。
    matrix wvp = mul(mul(World, View), Projection);
    matrix viewProjection = mul(View, Projection);

    VELOCITY_PS_IN Out;
    Out.Position = mul(currentPosition, wvp);
    Out.CurrentClip = Out.Position;
    Out.PreviousClip = mul(mul(previousPosition, PreviousWorld), viewProjection);
    return Out;
}
