#include "common.hlsl"

PS_IN main(in VSONESKIN_IN In)
{
    PS_IN Out;

	// �����X�L�����_�u�����h�̏���
    float4x4 comb = (float4x4) 0;
    for (int i = 0; i < 4; i++)
    {
		// �d�݂��v�Z���Ȃ���s�񐶐�
        comb += BoneMatrix[In.BoneIndex[i]] * In.BoneWeight[i];
    }

    float4 Pos;

    Pos = mul(In.Position,comb);
    In.Position = Pos;

	//
    matrix wvp;
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);
	
    float3x3 normalMatrix = Inverse3x3(float3x3(World._11, World._12, World._13,
                                               World._21, World._22, World._23,
                                               World._31, World._32, World._33));
    normalMatrix = transpose(normalMatrix);
    float3 worldNormal = normalize(mul(In.Normal.xyz, normalMatrix));
    float4 worldPosition = mul(In.Position, World);

    Out.Diffuse = In.Diffuse * Material.Diffuse;
    Out.Diffuse.a = In.Diffuse.a * Material.Diffuse.a;

    Out.Position = mul(In.Position, wvp);
    Out.TexCoord = In.TexCoord;
    Out.WorldNormal = worldNormal;
    Out.WorldPosition = worldPosition.xyz;
    Out.ShadowCoord = CalcShadowCoord(worldPosition);

    return Out;
}
