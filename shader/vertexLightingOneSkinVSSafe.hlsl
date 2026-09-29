#include "common.hlsl"

PS_IN main(in VSONESKIN_IN In)
{
    PS_IN Out;

    // FBXには、鎧の一部や小物などボーンウェイトを持たない頂点が
    // 混ざることがあります。BoneIndex=-1をGPU配列へ渡すと不正参照になり、
    // モデルが画面外へ飛ぶため、ここで必ず範囲チェックします。
    float4x4 comb = (float4x4)0;
    float4x4 identity = (float4x4)0;
    identity[0][0] = 1.0f;
    identity[1][1] = 1.0f;
    identity[2][2] = 1.0f;
    identity[3][3] = 1.0f;
    float weightSum = 0.0f;

    for (int i = 0; i < 4; ++i)
    {
        if (In.BoneIndex[i] >= 0 &&
            In.BoneIndex[i] < MAX_BONE &&
            In.BoneWeight[i] > 0.0f)
        {
            comb += BoneMatrix[In.BoneIndex[i]] * In.BoneWeight[i];
            weightSum += In.BoneWeight[i];
        }
    }

    if (weightSum > 0.0001f)
    {
        // 4ウェイトへの切り捨てで合計が1未満になった場合は、
        // 残りを恒等行列で補って頂点が縮まないようにします。
        if (weightSum < 0.9999f)
            comb += identity * (1.0f - weightSum);
    }
    else
    {
        // 完全に未スキンの頂点は、元のローカル座標を使用します。
        comb = identity;
    }

    In.Position = mul(In.Position, comb);

    matrix wvp = mul(mul(World, View), Projection);

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
