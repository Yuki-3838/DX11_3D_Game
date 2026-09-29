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
	float4 PBRParams; // x: Metallic, y: Roughness, z/w: reserved
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

// --- PBR用の視点・露出 ---
// CameraPosition.xyz: ワールド空間のカメラ位置
// ExposureParams.x: 露出。y/z/w: 予備
cbuffer ViewParamsBuffer : register(b9)
{
    float4 CameraPosition;
    float4 ExposureParams;
    // x: 明るさの持ち上げ量、y: 鏡面反射倍率、z: 粗さ上書き、w: 対象固有ディテール強度/暗部可読性補正
    float4 CharacterVisualParams;
    // rgb: ベースカラー倍率、a: 補正の適用量
    float4 CharacterAlbedoParams;
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
	float3 WorldNormal	: TEXCOORD2;
	float3 WorldPosition	: TEXCOORD3;
};

static const float PBR_PI = 3.14159265f;

float3 FresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0f - f0) * pow(1.0f - saturate(cosTheta), 5.0f);
}

float DistributionGGX(float nDotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denominator = nDotH * nDotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PBR_PI * denominator * denominator, 0.0001f);
}

float GeometrySchlickGGX(float nDot, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return nDot / max(nDot * (1.0f - k) + k, 0.0001f);
}

float GeometrySmith(float nDotV, float nDotL, float roughness)
{
    return GeometrySchlickGGX(nDotV, roughness) *
           GeometrySchlickGGX(nDotL, roughness);
}

float2 DragonScaleCell(float2 texCoord)
{
    // UVを細かい互い違いのセルへ分け、各セルの中央を鱗の山として扱う。
    // 既存のDiffuse UVを使うため、外部テクスチャを置き換えずに細部を追加できる。
    // 以前の密度では画面上で粒が細かすぎて、法線の変化が鱗として読めなかった。
    // 少し大きいセルにして、頭・胸・脚の各鱗の明暗を分離する。
    float2 grid = texCoord * float2(24.0f, 18.0f);
    float row = floor(grid.y);
    grid.x += frac(row * 0.5f);
    float2 cell = frac(grid) - 0.5f;
    cell.y *= 1.18f;
    return cell;
}

float DragonScaleHeight(float2 texCoord)
{
    float2 cell = DragonScaleCell(texCoord);
    float distanceToCenter = length(cell);
    float scale = 1.0f - smoothstep(0.14f, 0.46f, distanceToCenter);
    return pow(saturate(scale), 0.68f);
}

float DragonScaleEdge(float2 texCoord)
{
    float distanceToCenter = length(DragonScaleCell(texCoord));
    // 鱗の外周だけを細く抜き出す。谷を暗くし、隣の鱗との境界を作る。
    return smoothstep(0.20f, 0.30f, distanceToCenter) *
        (1.0f - smoothstep(0.34f, 0.46f, distanceToCenter));
}

float DragonScaleUpperRim(float2 texCoord)
{
    float2 cell = DragonScaleCell(texCoord);
    float upperWeight = saturate((-cell.y + 0.02f) * 3.6f);
    return DragonScaleEdge(texCoord) * upperWeight;
}

float DragonScaleLowerRim(float2 texCoord)
{
    float2 cell = DragonScaleCell(texCoord);
    float lowerWeight = saturate((cell.y + 0.02f) * 3.6f);
    return DragonScaleEdge(texCoord) * lowerWeight;
}

float3 ApplyDragonScaleNormal(float3 normal, float2 texCoord, float3 worldPosition,
                              float strength)
{
    float scaleHeight = DragonScaleHeight(texCoord);
    float heightDx = ddx(scaleHeight);
    float heightDy = ddy(scaleHeight);

    // 画面微分から安定した接平面を作り、鱗の高さ勾配をワールド法線へ加える。
    // Tangent頂点属性やスキニング経路を変更しないため、既存モデルへ安全に重ねられる。
    float3 tangent = ddx(worldPosition);
    tangent -= normal * dot(normal, tangent);
    tangent = normalize(tangent + float3(0.0001f, 0.0001f, 0.0001f));
    float3 bitangent = normalize(cross(normal, tangent));
    float3 detailNormal = normal - (heightDx * tangent + heightDy * bitangent) * strength;
    return normalize(detailNormal);
}

float3 ApplyDragonTextureNormal(float3 normal, float3 textureColor, float3 worldPosition,
                                float strength)
{
    // dragon.pngに描かれている実際の鱗の明暗も高さ情報として利用する。
    // 新しいノーマルマップを追加せず、既存アトラスのUVと完全に一致させる。
    float textureHeight = dot(saturate(textureColor), float3(0.30f, 0.59f, 0.11f));
    float heightDx = ddx(textureHeight);
    float heightDy = ddy(textureHeight);
    float3 positionDx = ddx(worldPosition);
    float3 positionDy = ddy(worldPosition);
    float3 detailNormal = normal -
        (heightDx * positionDy - heightDy * positionDx) * strength;
    return normalize(detailNormal);
}

float ResolveRoughness()
{
    // 未設定の既存マテリアルは、旧Shininessから極端に鏡面にならない値へ変換する。
    float fallback = 1.0f - saturate(Material.Shininess / 96.0f);
    if (Material.Shininess <= 0.001f)
        fallback = 0.65f;
    return max(saturate(Material.PBRParams.y > 0.001f
        ? Material.PBRParams.y
        : fallback), 0.08f);
}

float3 EvaluateDirectionalPBR(float3 baseColor, float3 normal, float3 worldPosition,
                              float3 textureColor, float2 texCoord, float shadowFactor)
{
    float albedoTintWeight = saturate(CharacterAlbedoParams.a);
    float3 albedoMultiplier = lerp(
        float3(1.0f, 1.0f, 1.0f),
        max(CharacterAlbedoParams.rgb, 0.0f),
        albedoTintWeight);
    baseColor *= albedoMultiplier;
    // プレイヤーの黒い防具は線形空間のまま倍率を掛けても暗部が潰れやすい。
    // wが有効な描画対象だけ、平方根カーブを弱く混ぜて暗部の階調を持ち上げる。
    // 露出・環境光には触れないため、敵・床・壁へは影響しない。
    float playerVisual = step(0.001f, CharacterVisualParams.x);
    float dragonVisual = (1.0f - playerVisual) * step(0.001f, CharacterVisualParams.y);
    float readability = saturate(CharacterVisualParams.w) * playerVisual;
    baseColor = lerp(baseColor, sqrt(max(baseColor, 0.0f)), readability);
    // ドラゴンは法線だけでなく、鱗の山と谷をベースカラーにも弱く反映する。
    // これで光の向きに左右されず、鱗一枚ごとの境界を画面上で読める。
    float dragonScaleHeight = DragonScaleHeight(texCoord);
    float dragonDetailWeight = saturate(dragonVisual * CharacterVisualParams.w * 1.45f);
    float3 scaleTone = lerp(
        float3(0.58f, 0.50f, 0.46f),
        float3(1.16f, 1.08f, 1.00f),
        dragonScaleHeight);
    baseColor *= lerp(
        float3(1.0f, 1.0f, 1.0f),
        scaleTone,
        dragonDetailWeight);
    float scaleEdge = DragonScaleEdge(texCoord);
    float scaleUpperRim = DragonScaleUpperRim(texCoord);
    float scaleLowerRim = DragonScaleLowerRim(texCoord);
    baseColor *= lerp(
        float3(1.0f, 1.0f, 1.0f),
        float3(0.70f, 0.60f, 0.54f),
        scaleEdge * dragonDetailWeight * 0.85f);
    baseColor += float3(0.075f, 0.040f, 0.018f) *
        scaleUpperRim * dragonDetailWeight;
    baseColor *= 1.0f - scaleLowerRim * dragonDetailWeight * 0.18f;
    // 鱗用プロファイルでは、白い鏡面反射をそのまま残さず、
    // ベースカラーに寄った反射色にする。黒い鱗の上でも白く塗り潰れにくい。
    float3 scaleSpecularTint = lerp(
        float3(1.0f, 1.0f, 1.0f),
        saturate(baseColor * 1.8f + float3(0.02f, 0.012f, 0.008f)),
        albedoTintWeight);

    float3 N = normalize(normal);
    // ドラゴンだけに鱗の一枚ごとの凹凸を加える。プレイヤーの可読性補正とは
    // 別フラグで扱い、敵のマテリアルだけへ作用させる。
    N = ApplyDragonScaleNormal(N, texCoord, worldPosition, 0.30f * dragonVisual *
        saturate(CharacterVisualParams.w));
    N = ApplyDragonTextureNormal(N, textureColor, worldPosition,
        3.50f * dragonDetailWeight);
    float3 L = normalize(-Light.Direction.xyz);
    float3 V = normalize(CameraPosition.xyz - worldPosition);
    float3 H = normalize(V + L);

    float nDotL = saturate(dot(N, L));
    float nDotV = saturate(dot(N, V));
    float nDotH = saturate(dot(N, H));
    float vDotH = saturate(dot(V, H));
    float metallic = saturate(Material.PBRParams.x);
    float roughnessOverride = CharacterVisualParams.z;
    float roughness = roughnessOverride > 0.001f
        ? max(saturate(roughnessOverride), 0.08f)
        : ResolveRoughness();
    // 鱗の谷は粗く、山は少し滑らかにして、同じパターンでハイライトも分割する。
    roughness = lerp(
        roughness,
        lerp(0.94f, 0.70f, dragonScaleHeight),
        dragonDetailWeight * 0.75f);

    // 旧PhongのSpecularは「反射率」ではなく、白いハイライトの強さとして
    // 格納されているアセットが多い。そのままF0へ使うと、鎧も鱗も白い
    // プラスチックのように見えるため、非金属の誘電体F0へ安全に寄せる。
    // glTFのMetallicが指定されている場合は、下のlerpでBaseColorを使う。
    float3 legacySpecular = saturate(Material.Specular.rgb);
    float3 legacyF0 = lerp(
        float3(0.04f, 0.04f, 0.04f),
        float3(0.08f, 0.08f, 0.08f),
        saturate(legacySpecular * 0.5f));
    float3 f0 = lerp(legacyF0, baseColor, metallic);
    float3 F = FresnelSchlick(vDotH, f0) * scaleSpecularTint;
    float D = DistributionGGX(nDotH, roughness);
    float G = GeometrySmith(nDotV, nDotL, roughness);
    float3 specular = (D * G * F) /
        max(4.0f * nDotV * nDotL, 0.0001f);

    float3 kD = (1.0f - F) * (1.0f - metallic);
    float3 diffuse = kD * baseColor / PBR_PI;
    float3 directDiffuse = diffuse * Light.Diffuse.rgb * nDotL * shadowFactor;
    float specularScale = CharacterVisualParams.y > 0.001f
        ? CharacterVisualParams.y
        : 1.0f;
    float3 directSpecular = specular * Light.Diffuse.rgb * nDotL * shadowFactor
        * max(specularScale, 0.20f);

    // IBL導入前の最低限の環境項。シーン全体の明るさを変えないよう、
    // 環境光はLight.Ambientの値だけを使う。
    float3 ambient = baseColor * Light.Ambient.rgb * kD;
    float3 playerBrightnessLift = baseColor * max(CharacterVisualParams.x, 0.0f);
    // 見下ろし視点で正面が逆光になったときも、鎧の輪郭が背景へ埋もれないようにする。
    float rim = pow(1.0f - nDotV, 2.2f) * readability;
    float3 playerRimLight = float3(0.14f, 0.17f, 0.22f) * rim;
    return directDiffuse + directSpecular + ambient + playerBrightnessLift + playerRimLight;
}

float3 ToneMapPBR(float3 hdrColor)
{
    float exposure = max(ExposureParams.x, 0.01f);
    float3 mapped = 1.0f - exp(-max(hdrColor, 0.0f) * exposure);
    // バックバッファはUNORMなので、ここで線形値をsRGB相当へ戻す。
    return pow(saturate(mapped), 1.0f / 2.2f);
}

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
