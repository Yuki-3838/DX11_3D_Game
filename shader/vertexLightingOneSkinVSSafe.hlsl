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

    float4 worldNormal = mul(float4(In.Normal.xyz, 0.0f), World);
    worldNormal = normalize(worldNormal);
    float light = -(dot(Light.Direction.xyz, worldNormal.xyz)) * 0.5f + 0.5f;
    light = saturate(light);

    Out.Diffuse = In.Diffuse * Material.Diffuse * light * Light.Diffuse;
    Out.Diffuse += In.Diffuse * Material.Ambient * Light.Ambient;
    // Armored characters have large nearly-black surfaces.  Keep a small
    // neutral fill term so the silhouette and plate details remain readable
    // even when the directional light is behind the player.
    Out.Diffuse += In.Diffuse * Material.Diffuse * 0.16f;
    Out.Diffuse += Material.Emission;
    Out.Diffuse.a = In.Diffuse.a * Material.Diffuse.a;

    Out.Position = mul(In.Position, wvp);
    Out.TexCoord = In.TexCoord;
    return Out;
}
