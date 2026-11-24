#include "common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

float4 main(in SKIN_PS_IN In) : SV_Target
{
    // 影は光源から届く明るさにだけ掛ける(環境光や底上げは残す)。自分自身にも落ちるので、
    // 背中のとげが胴体へ落とす影や、腕が胴へ落とす影なども出る。
    // 光の裏側の面は ShadowLoss が0で、陰影の計算だけで暗くなる
    // (以前は色全体に影を掛けていたため、裏側が二重に暗くなり、境目に縞が出ていた)。
    float4 lighting = In.Diffuse;
    lighting.rgb -= In.ShadowLoss.rgb * (1.0f - CalcShadowVisibility(In.ShadowCoord));

    float4 outDiffuse;
    if (Material.TextureEnable)
    {
        outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
        outDiffuse *= lighting;
    }
    else
    {
        outDiffuse = lighting;
    }

    // The paladin uses very dark plate textures.  A small post-texture lift
    // keeps the armor silhouette and material details visible in gameplay,
    // without flattening the directional shading in the vertex shader.
    outDiffuse.rgb = saturate(outDiffuse.rgb + float3(0.08f, 0.08f, 0.08f));

    // 被弾フラッシュ。影を掛けた後に足すことで、影の中でも光って見える。
    // 命中した瞬間が分からないと手応えが出ないため、暗部でも視認できるようにする。
    outDiffuse.rgb = saturate(outDiffuse.rgb + CharacterTint.rgb * CharacterTint.a);

    outDiffuse.a = In.Diffuse.a;
    return outDiffuse;
}
