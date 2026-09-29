#include "common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

float4 main(in PS_IN In) : SV_Target
{
    float4 outDiffuse;
    float3 textureColor = float3(1.0f, 1.0f, 1.0f);
    if (Material.TextureEnable)
    {
        outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
        textureColor = outDiffuse.rgb;
        outDiffuse *= In.Diffuse;
    }
    else
    {
        outDiffuse = In.Diffuse;
    }

    // テクスチャだけを線形化し、頂点/マテリアル色を二重に暗くしない。
    float3 baseColor = Material.TextureEnable
        ? pow(saturate(textureColor), 2.2f)
            * saturate(In.Diffuse.rgb)
        : saturate(In.Diffuse.rgb);
    float3 hdrColor = EvaluateDirectionalPBR(
        baseColor,
        In.WorldNormal,
        In.WorldPosition,
        textureColor,
        In.TexCoord,
        CalcShadowFactor(In.ShadowCoord));
    hdrColor += Material.Emission.rgb;

    // 被弾フラッシュはトーンマッピング前に加算し、暗部でも視認できるようにする。
    hdrColor += CharacterTint.rgb * CharacterTint.a;

    return float4(ToneMapPBR(hdrColor), outDiffuse.a);
}
