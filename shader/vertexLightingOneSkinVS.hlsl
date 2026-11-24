#include "common.hlsl"

SKIN_PS_IN main(in VSONESKIN_IN In)
{
    SKIN_PS_IN Out;

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

    // 直接光の分だけをピクセルシェーダーで影に掛ける(SKIN_PS_IN の説明を参照)。
    const float2 lightParts = SplitHalfLambert(light);
    Out.Diffuse = In.Diffuse * Material.Diffuse * lightParts.x * Light.Diffuse;
    Out.SunLight = In.Diffuse * Material.Diffuse * lightParts.y * Light.Diffuse;
    Out.SunLight.a = 0.0f;
    Out.Diffuse += In.Diffuse * Material.Ambient * Light.Ambient;
    Out.Diffuse += Material.Emission;
    Out.Diffuse.a = In.Diffuse.a * Material.Diffuse.a;

    Out.Position = mul(In.Position, wvp);
    Out.TexCoord = In.TexCoord;
    Out.ShadowCoord = CalcShadowCoordOffset(mul(In.Position, World), worldNormal.xyz);

    return Out;
}
