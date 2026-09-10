#include "common.hlsl"

PS_IN main(in VS_IN In)
{
    PS_IN Out;
	
	matrix wvp;
	wvp = mul(World, View);
	wvp = mul(wvp, Projection);

	Out.Position = mul(In.Position, wvp);
	Out.TexCoord = In.TexCoord;
	Out.Diffuse = In.Diffuse * Material.Diffuse;
	// このシェーダーは無照明だが、PS_INの全要素を埋めておかないと
	// ピクセルシェーダー側で未定義の値を読むことになるため設定する。
	Out.ShadowCoord = CalcShadowCoord(mul(In.Position, World));

    return Out;
}

