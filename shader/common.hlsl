cbuffer WorldBuffer : register(b0)
{
	matrix World;
}
cbuffer ViewBuffer : register(b1)
{
	matrix View;
}
cbuffer ProjectionBuffer : register(b2)
{
	matrix Projection;
}

struct MATERIAL
{
	float4 Ambient;
	float4 Diffuse;
	float4 Specular;
	float4 Emission;
	float Shininess;
	bool TextureEnable;
	float2 Dummy;
};

cbuffer MaterialBuffer : register(b3)
{
	MATERIAL Material;
}

struct LIGHT
{
	bool Enable;					// �g�p���邩�ۂ�
	bool3 Dummy;					// PADDING
	float4 Direction;				// ����
	float4 Diffuse;					// �g�U���˗p�̌��̋���
	float4 Ambient;					// �����p�̌��̋���
};

cbuffer LightBuffer : register(b4)
{
    LIGHT Light;
};

#define MAX_BONE 800
cbuffer BoneMatrixBuffer : register(b5)
{
    matrix BoneMatrix[MAX_BONE];
}

cbuffer LINEWIDTH : register(b6)
{
    float LineWidth;
    float3 PADDING;
};

// --- シャドウマッピング ---
// ライトから見たビュー射影行列と、影の参照設定。
// ShadowParams.x: シャドウマップ1テクセルのUVサイズ(PCFの参照間隔)
// ShadowParams.y: 影を有効にするか(1で有効)
cbuffer ShadowBuffer : register(b7)
{
    matrix LightViewProjection;
    float4 ShadowParams;
};

// --- キャラクターの一時的な色変化(被弾フラッシュなど) ---
// rgb: 加算する色。a: 強さ(0で無効)。
// メッシュのマテリアルはサブセットごとに設定されるため、描画前に
// マテリアルを差し替える方法では上書きされてしまう。そこで専用の定数バッファを使う。
cbuffer TintBuffer : register(b8)
{
    float4 CharacterTint;
};

Texture2D g_ShadowMap : register(t1);
// 深度比較をハードウェアに行わせる比較サンプラー。
// 4テクセル分の比較結果を自動で補間してくれるため、輪郭のジャギーが和らぐ。
SamplerComparisonState g_ShadowSampler : register(s1);


struct VS_IN
{
	float4 Position		: POSITION0;
	float4 Normal		: NORMAL0;
	float4 Diffuse		: COLOR0;
	float2 TexCoord		: TEXCOORD0;
};

struct VSONESKIN_IN
{
    float4 Position		: POSITION0;
    float4 Normal		: NORMAL0;
    float4 Diffuse		: COLOR0;
    float2 TexCoord		: TEXCOORD0;
    int4   BoneIndex	: BONEINDEX;
    float4 BoneWeight	: BONEWEIGHT;
};

struct PS_IN
{
	float4 Position		: SV_POSITION;
	float4 Diffuse		: COLOR0;
	float2 TexCoord		: TEXCOORD0;
	// ライト空間での位置。頂点シェーダーで求めてピクセルシェーダーへ渡す。
	// 頂点単位で影を計算すると、大きな三角形(床など)で影が崩れるため、
	// 判定はピクセル単位で行う。
	float4 ShadowCoord	: TEXCOORD1;
};

/**
 * ワールド座標からライト空間の座標を求める。各頂点シェーダーの末尾で呼ぶ。
 */
float4 CalcShadowCoord(float4 worldPosition)
{
    return mul(worldPosition, LightViewProjection);
}

/**
 * 影の減光量を求める。1.0で影なし、小さいほど濃い影。
 * 3x3のPCF(Percentage Closer Filtering)で、輪郭を少しぼかす。
 */
float CalcShadowFactor(float4 shadowCoord)
{
    // 影が無効なら常に影なし。
    if (ShadowParams.y < 0.5f)
        return 1.0f;

    // 射影除算してクリップ空間へ。wが0以下の場合はライトの裏側なので影を落とさない。
    if (shadowCoord.w <= 0.0f)
        return 1.0f;
    float3 projected = shadowCoord.xyz / shadowCoord.w;

    // クリップ空間(-1..1)からテクスチャのUV(0..1)へ。Yは上下が逆になる。
    float2 shadowUv = float2(projected.x * 0.5f + 0.5f, -projected.y * 0.5f + 0.5f);

    // シャドウマップの範囲外は影なしとして扱う。
    if (shadowUv.x < 0.0f || shadowUv.x > 1.0f ||
        shadowUv.y < 0.0f || shadowUv.y > 1.0f ||
        projected.z > 1.0f)
    {
        return 1.0f;
    }

    const float texelSize = ShadowParams.x;
    float lit = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 offset = float2(x, y) * texelSize;
            lit += g_ShadowMap.SampleCmpLevelZero(
                g_ShadowSampler, shadowUv + offset, projected.z);
        }
    }
    lit /= 9.0f;

    // 完全な黒にはせず、影の中でも形が見える程度の明るさを残す。
    return lerp(0.45f, 1.0f, lit);
}

// 3x3 �t�s����v�Z����֐�
float3x3 Inverse3x3(float3x3 m)
{
    float a = m[0].x, b = m[0].y, c = m[0].z;
    float d = m[1].x, e = m[1].y, f = m[1].z;
    float g = m[2].x, h = m[2].y, i = m[2].z;

    float A = e * i - f * h;
    float B = f * g - d * i;
    float C = d * h - e * g;

    float det = a * A + b * B + c * C;
    if (abs(det) < 1e-6)
    {
        float3x3 zeroMatrix = float3x3(0.0f, 0.0f, 0.0f,
                              0.0f, 0.0f, 0.0f,
                              0.0f, 0.0f, 0.0f);
        return zeroMatrix; // �񐳑��Ȃ�[���s��       
    }

    float invDet = 1.0 / det;

    return float3x3(
         A, c * h - b * i, b * f - c * e,
         B, a * i - c * g, c * d - a * f,
         C, b * g - a * h, a * e - b * d
    ) * invDet;
}