#include "common.hlsl"
#include "skinTransform.hlsli"

// 影生成パスも本描画と同じLBS/DQSハイブリッドで頂点位置を計算する。
// 影だけLBSにすると、DQSで変形した脚などの輪郭と影が一致しない。

struct SHADOW_PS_IN
{
    float4 Position : SV_POSITION;
};

SHADOW_PS_IN main(in VSONESKIN_IN In)
{
    SHADOW_PS_IN Out;
    float4 skinnedPosition = SkinPositionHybrid(In);
    float4 worldPosition = mul(skinnedPosition, World);
    Out.Position = mul(worldPosition, LightViewProjection);
    return Out;
}
