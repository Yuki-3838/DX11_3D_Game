// 本描画と影描画で共有するスキニング処理。
// 行ベクトル規約(pos = mul(pos, M))に合わせている。

// DQSの混合比率。0.0で完全なLBS、1.0で完全なDQS。
static const float DQS_BLEND = 0.75f;

// 剛体変換行列から回転クォータニオンを取り出す。
// 戻り値は (x, y, z, w)。
float4 QuaternionFromMatrix(float4x4 m)
{
    float trace = m[0][0] + m[1][1] + m[2][2];
    float4 q;

    if (trace > 0.0f)
    {
        float s = sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m[1][2] - m[2][1]) / s;
        q.y = (m[2][0] - m[0][2]) / s;
        q.z = (m[0][1] - m[1][0]) / s;
    }
    else if (m[0][0] > m[1][1] && m[0][0] > m[2][2])
    {
        float s = sqrt(1.0f + m[0][0] - m[1][1] - m[2][2]) * 2.0f;
        q.w = (m[1][2] - m[2][1]) / s;
        q.x = 0.25f * s;
        q.y = (m[1][0] + m[0][1]) / s;
        q.z = (m[2][0] + m[0][2]) / s;
    }
    else if (m[1][1] > m[2][2])
    {
        float s = sqrt(1.0f + m[1][1] - m[0][0] - m[2][2]) * 2.0f;
        q.w = (m[2][0] - m[0][2]) / s;
        q.x = (m[1][0] + m[0][1]) / s;
        q.y = 0.25f * s;
        q.z = (m[2][1] + m[1][2]) / s;
    }
    else
    {
        float s = sqrt(1.0f + m[2][2] - m[0][0] - m[1][1]) * 2.0f;
        q.w = (m[0][1] - m[1][0]) / s;
        q.x = (m[2][0] + m[0][2]) / s;
        q.y = (m[2][1] + m[1][2]) / s;
        q.z = 0.25f * s;
    }
    return q;
}

// クォータニオンの積。(x, y, z, w) 表現。
float4 QuaternionMultiply(float4 a, float4 b)
{
    return float4(
        a.w * b.xyz + b.w * a.xyz + cross(a.xyz, b.xyz),
        a.w * b.w - dot(a.xyz, b.xyz));
}

// クォータニオンでベクトルを回す。
float3 RotateByQuaternion(float4 q, float3 v)
{
    return v + 2.0f * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

// スキニング結果。位置と法線を同じ計算から得る。
struct SkinnedVertex
{
    float4 position;
    float3 normal;
};

/**
 * 本描画と影描画で共有するLBS/DQSハイブリッドのスキニング。
 *
 * 法線も同じボーンの重みで変形させる。以前は法線をワールド行列だけで変換しており、
 * ボーンをどれだけ動かしても陰影が追従しなかった(体を捻っても明暗が変わらない)。
 * DQS側は剛体回転なので、混合した回転クォータニオンで法線を回すだけでよい。
 */
SkinnedVertex SkinVertexHybrid(VSONESKIN_IN In, float3 localNormal)
{
    float4x4 identity = (float4x4) 0;
    identity[0][0] = 1.0f;
    identity[1][1] = 1.0f;
    identity[2][2] = 1.0f;
    identity[3][3] = 1.0f;

    float4x4 lbsMatrix = (float4x4) 0;
    float weightSum = 0.0f;
    float4 blendedReal = (float4) 0;
    float4 blendedDual = (float4) 0;
    float4 referenceQuat = (float4) 0;
    bool hasReference = false;

    for (int i = 0; i < 4; ++i)
    {
        if (In.BoneIndex[i] < 0 ||
            In.BoneIndex[i] >= MAX_BONE ||
            In.BoneWeight[i] <= 0.0f)
        {
            continue;
        }

        float4x4 boneMatrix = BoneMatrix[In.BoneIndex[i]];
        float weight = In.BoneWeight[i];
        lbsMatrix += boneMatrix * weight;
        weightSum += weight;

        float4 real = normalize(QuaternionFromMatrix(boneMatrix));
        if (!hasReference)
        {
            referenceQuat = real;
            hasReference = true;
        }
        else if (dot(referenceQuat, real) < 0.0f)
        {
            real = -real;
        }

        float3 translation = boneMatrix[3].xyz;
        float4 dual = 0.5f * QuaternionMultiply(float4(translation, 0.0f), real);
        blendedReal += real * weight;
        blendedDual += dual * weight;
    }

    SkinnedVertex result;
    if (weightSum <= 0.0001f)
    {
        // ウェイトを持たない頂点は元のまま。
        result.position = In.Position;
        result.normal = localNormal;
        return result;
    }

    // 4ウェイトへの切り捨てで不足した分を恒等変換で補う。
    float residual = saturate(1.0f - weightSum);
    if (residual > 0.0f)
    {
        lbsMatrix += identity * residual;
        float4 identityReal = float4(0.0f, 0.0f, 0.0f, 1.0f);
        if (hasReference && dot(referenceQuat, identityReal) < 0.0f)
            identityReal = -identityReal;
        blendedReal += identityReal * residual;
    }

    float4 lbsPosition = mul(In.Position, lbsMatrix);
    // 法線は平行移動の影響を受けないので、行列の回転部分だけを使う。
    float3 lbsNormal = mul(localNormal, (float3x3) lbsMatrix);

    float realLength = length(blendedReal);
    float4 dqsPosition = lbsPosition;
    float3 dqsNormal = lbsNormal;
    if (realLength > 0.0001f)
    {
        float4 real = blendedReal / realLength;
        float4 dual = blendedDual / realLength;
        float3 position = In.Position.xyz;
        float3 rotated = RotateByQuaternion(real, position);
        float3 translated =
            2.0f * (real.w * dual.xyz - dual.w * real.xyz + cross(real.xyz, dual.xyz));
        dqsPosition = float4(rotated + translated, In.Position.w);
        // DQSは剛体変換なので、法線は同じ回転を掛けるだけでよい。
        dqsNormal = RotateByQuaternion(real, localNormal);
    }

    result.position = lerp(lbsPosition, dqsPosition, DQS_BLEND);
    float3 blendedNormal = lerp(lbsNormal, dqsNormal, DQS_BLEND);
    // 補間で長さが崩れるため正規化する。ゼロ長になった場合は元の法線へ戻す。
    float normalLength = length(blendedNormal);
    result.normal = normalLength > 0.0001f
        ? blendedNormal / normalLength
        : localNormal;
    return result;
}

// 位置だけが必要な場合(影生成パスなど)。
float4 SkinPositionHybrid(VSONESKIN_IN In)
{
    return SkinVertexHybrid(In, float3(0.0f, 1.0f, 0.0f)).position;
}
