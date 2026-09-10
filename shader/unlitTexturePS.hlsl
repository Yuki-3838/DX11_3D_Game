#include "common.hlsl"

Texture2D		g_Texture : register(t0);
SamplerState	g_SamplerState : register(s0);

float4 main(in PS_IN In) : SV_Target
{
    float4 outDiffuse;
	
	if (Material.TextureEnable)
	{
		outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
		outDiffuse *= In.Diffuse;
	}
	else
	{
		outDiffuse = In.Diffuse;
	}

	// 無照明の壁・床でも、キャラクターが落とす影は受ける。
	// 無照明は面の向きによる明暗を無効にするだけで、シャドウ判定まで
	// 無効にすると壁だけ影が抜けてしまう。
	outDiffuse.rgb *= CalcShadowFactor(In.ShadowCoord);

    return outDiffuse;
}
