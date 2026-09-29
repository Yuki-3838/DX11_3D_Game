#include "common.hlsl"
#include "skinTransform.hlsli"

// =============================================================================
// LBS / DQS ハイブリッドスキニング
//
// スキニング後の位置は、影描画パスと共有する SkinPositionHybrid で計算する。
// 本体と影で異なる方式を使うと、特に脚の輪郭と接地影がずれるためである。
// 詳しい技術的背景と選定理由は docs/スキニング技術解説.md を参照。
// =============================================================================

PS_IN main(in VSONESKIN_IN In)
{
    PS_IN Out;
    // 位置と法線を同じスキニングから求める。
    // 以前は法線をワールド行列だけで変換していたため、ボーンを動かしても
    // 陰影が追従せず、体を捻っても明暗が変わらなかった。
    SkinnedVertex skinned = SkinVertexHybrid(In, In.Normal.xyz);
    float4 skinnedPosition = skinned.position;
    matrix wvp = mul(mul(World, View), Projection);

    float3x3 normalMatrix = Inverse3x3(float3x3(World._11, World._12, World._13,
                                               World._21, World._22, World._23,
                                               World._31, World._32, World._33));
    normalMatrix = transpose(normalMatrix);
    float3 worldNormal = normalize(mul(skinned.normal, normalMatrix));
    float4 worldPosition = mul(skinnedPosition, World);

    Out.Diffuse = In.Diffuse * Material.Diffuse;
    Out.Diffuse.a = In.Diffuse.a * Material.Diffuse.a;

    Out.Position = mul(skinnedPosition, wvp);
    Out.TexCoord = In.TexCoord;
    Out.WorldNormal = worldNormal;
    Out.WorldPosition = worldPosition.xyz;
    Out.ShadowCoord = CalcShadowCoord(worldPosition);
    return Out;
}
