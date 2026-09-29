#include "common.hlsl"

PS_IN main(in VS_IN In)
{
    PS_IN Out;

	matrix wvp;
	wvp = mul(World, View);
	wvp = mul(wvp, Projection);
	
    // �@���ϊ��s����v�Z�i�g�k��������菜���j
    float3x3 normalMatrix = Inverse3x3(float3x3(World._11, World._12, World._13,
                                             World._21, World._22, World._23,
                                             World._31, World._32, World._33));
    // �]�u
    normalMatrix = transpose(normalMatrix);

    // �@���x�N�g���̕��������[���h���W�n�ɕϊ�
	float3 worldNormal = normalize(mul(In.Normal.xyz, normalMatrix));
	float4 worldPosition = mul(In.Position, World);

	// PBRはピクセル単位で計算するため、ここでは材質のベース色だけ渡す。
	Out.Diffuse = In.Diffuse * Material.Diffuse;
	Out.Diffuse.a = In.Diffuse.a * Material.Diffuse.a;
	
	Out.Position = mul( In.Position, wvp );
	Out.TexCoord = In.TexCoord;
	Out.WorldNormal = worldNormal;
	Out.WorldPosition = worldPosition.xyz;
	// 影の判定用に、ライトから見た位置も渡す。
	Out.ShadowCoord = CalcShadowCoord(worldPosition);

    return Out;
}

