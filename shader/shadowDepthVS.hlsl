#include "common.hlsl"

// 影生成パス用(スキニングなし)の頂点シェーダー。
// ライトから見た深度だけを書き込むので、色や法線は計算しない。

struct SHADOW_PS_IN
{
    float4 Position : SV_POSITION;
};

SHADOW_PS_IN main(in VS_IN In)
{
    SHADOW_PS_IN Out;
    // ワールド変換した後、ライトのビュー射影で変換する。
    float4 worldPosition = mul(In.Position, World);
    Out.Position = mul(worldPosition, LightViewProjection);
    return Out;
}
