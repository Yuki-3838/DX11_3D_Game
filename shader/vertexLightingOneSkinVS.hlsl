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
	
    float4 worldNormal, normal;
    normal = float4(In.Normal.xyz, 0.0);
    worldNormal = mul(normal, World);
    worldNormal = normalize(worldNormal);

    float light = -(dot(Light.Direction.xyz, worldNormal.xyz))* 0.5 + 0.5;
    light = saturate(light);  // �����o�[�g���˂̌v�Z

    Out.Diffuse = In.Diffuse * Material.Diffuse * light * Light.Diffuse;
    Out.Diffuse += In.Diffuse * Material.Ambient * Light.Ambient;
    Out.Diffuse += Material.Emission;
    Out.Diffuse.a = In.Diffuse.a * Material.Diffuse.a;

    Out.Position = mul(In.Position, wvp);
    Out.TexCoord = In.TexCoord;
    Out.ShadowCoord = CalcShadowCoord(mul(In.Position, World));

    return Out;
}
