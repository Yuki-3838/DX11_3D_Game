#include "common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

float4 main(in PS_IN In)  : SV_Target
{	
	float4 baseSample;
	float3 textureColor = float3(1.0f, 1.0f, 1.0f);

	if (Material.TextureEnable)
	{
		baseSample = g_Texture.Sample(g_SamplerState, In.TexCoord);
		textureColor = baseSample.rgb;
		baseSample *= In.Diffuse;
   }
	else
	{
		baseSample = In.Diffuse;
    }

	// 画像ファイルはUNORMで読み込まれているため、PBR入力前に線形化する。
	// テクスチャはUNORMのsRGB画像、頂点/マテリアル色は線形値として扱う。
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
	hdrColor += CharacterTint.rgb * CharacterTint.a;

	return float4(ToneMapPBR(hdrColor), baseSample.a);
}
