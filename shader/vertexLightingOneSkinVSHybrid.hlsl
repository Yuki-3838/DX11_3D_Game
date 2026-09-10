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

    float4 worldNormal = mul(float4(skinned.normal, 0.0f), World);
    worldNormal = normalize(worldNormal);
    float light = -(dot(Light.Direction.xyz, worldNormal.xyz)) * 0.5f + 0.5f;
    light = saturate(light);

    Out.Diffuse = In.Diffuse * Material.Diffuse * light * Light.Diffuse;
    Out.Diffuse += In.Diffuse * Material.Ambient * Light.Ambient;
    // 鎧のキャラクターは黒に近い面が広いので、逆光でもシルエットと
    // 装甲のディテールが読めるように弱い環境項を残す。
    Out.Diffuse += In.Diffuse * Material.Diffuse * 0.16f;
    Out.Diffuse += Material.Emission;
    Out.Diffuse.a = In.Diffuse.a * Material.Diffuse.a;

    Out.Position = mul(skinnedPosition, wvp);
    Out.TexCoord = In.TexCoord;
    Out.ShadowCoord = CalcShadowCoord(mul(skinnedPosition, World));
    return Out;
}
