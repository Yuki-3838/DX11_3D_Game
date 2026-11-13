#include "motionBlurCommon.hlsli"

// 速度バッファから画面へブラーを掛ける全画面パス。
//
// 動いているキャラクターの輪郭の外(背景のピクセル)にも色をにじませるには、
// 「近くに自分まで届く速さのピクセルがあるか」を知る必要がある。全ピクセルで広い範囲を探すと重いので、
// 画面をタイルに分けて、タイルごとの最大の速度を先に求める。
//   1. PSTileMax     … タイルの中で一番速い速度
//   2. PSNeighborMax … 周囲のタイルも含めて一番速い速度
//   3. PSReconstruct … その速度の方向に沿って周りの色を集める
//
// ブラーの上限はタイル2枚分なので、周囲2タイルまで見れば届くピクセルを見落とさない。

Texture2D<float4> g_Velocity : register(t2);
Texture2D<float2> g_TileVelocity : register(t3);
Texture2D<float4> g_Scene : register(t4);
// 色はピクセルの間も補間して取る。最も近いピクセルだけだと、動きの方向に櫛のような段差が出る。
SamplerState g_LinearClamp : register(s2);

struct FULLSCREEN_OUT
{
    float4 Position : SV_POSITION;
};

// 頂点バッファを使わない、画面を覆う大きな三角形。
FULLSCREEN_OUT VSFullscreen(uint vertexId : SV_VertexID)
{
    const float2 corner = float2((vertexId << 1) & 2, vertexId & 2);
    FULLSCREEN_OUT Out;
    Out.Position = float4(corner * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return Out;
}

float2 PSTileMax(FULLSCREEN_OUT In) : SV_Target
{
    const int2 origin = int2(In.Position.xy) * TileSize;
    float2 fastest = float2(0.0f, 0.0f);
    float fastestSpeed = 0.0f;
    [loop]
    for (int y = 0; y < TileSize; ++y)
    {
        [loop]
        for (int x = 0; x < TileSize; ++x)
        {
            // 画面の外は0が返るので、端のタイルでも範囲を確かめなくてよい。
            const float2 velocity = g_Velocity.Load(int3(origin + int2(x, y), 0)).xy;
            const float speed = dot(velocity, velocity);
            if (speed > fastestSpeed)
            {
                fastestSpeed = speed;
                fastest = velocity;
            }
        }
    }
    return fastest;
}

float2 PSNeighborMax(FULLSCREEN_OUT In) : SV_Target
{
    const int2 tile = int2(In.Position.xy);
    float2 fastest = float2(0.0f, 0.0f);
    float fastestSpeed = 0.0f;
    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            const float2 velocity = g_TileVelocity.Load(int3(tile + int2(x, y), 0));
            const float speed = dot(velocity, velocity);
            if (speed > fastestSpeed)
            {
                fastestSpeed = speed;
                fastest = velocity;
            }
        }
    }
    return fastest;
}

// 色を集める点の数。少ないと細い剣の筋が点線に見える。
static const int SAMPLE_COUNT = 25;
// 前後関係を判定するときの幅(ワールド単位)。これより近い深度は同じ面として扱う。
static const float SOFT_DEPTH = 3.0f;

// 点を置く位置をピクセルごとにずらす乱数。全ピクセルで同じ位置だと筋状の模様が見える。
float InterleavedGradientNoise(float2 pixel)
{
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

// 片側の長さがspeedのブラーが、距離distanceの場所をどれだけ覆うか。
// 距離で積分すると1になるように割っている(動きの全時間のうち、その場所を覆っている割合になる)。
float BlurCoverage(float distance, float speed)
{
    // ほとんど動いていないピクセルは、中心の色の重みとして別に扱う。
    if (speed <= 1.0f)
        return 0.0f;
    return saturate(1.0f - distance / speed) / speed;
}

float4 PSReconstruct(FULLSCREEN_OUT In) : SV_Target
{
    const int2 pixel = int2(In.Position.xy);
    const float4 center = g_Scene.Load(int3(pixel, 0));
    const float3 self = g_Velocity.Load(int3(pixel, 0)).xyz;

    if (DebugView != 0)
    {
        // 調整用: 速度を色で見る(赤が横、緑が縦)。
        const float2 shown = abs(self.xy) / MaxBlurRadius;
        return float4(lerp(center.rgb * 0.25f, float3(shown, 0.0f), saturate(length(shown) * 4.0f)), 1.0f);
    }

    const uint2 tile = uint2(pixel) / uint(TileSize);
    const float2 direction = g_TileVelocity.Load(int3(tile, 0));
    const float reach = length(direction);
    // 近くに動いているものが無ければ何もしない(画面の大半はここで終わる)。
    if (reach < 0.5f)
        return center;

    const float selfSpeed = length(self.xy);
    const float noise = InterleavedGradientNoise(In.Position.xy);
    const float spacing = 2.0f * reach / SAMPLE_COUNT;
    const int2 lastPixel = int2(ViewportSize) - 1;

    float3 collected = float3(0.0f, 0.0f, 0.0f);
    float collectedWeight = 0.0f;
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        const float t = (i + noise) / SAMPLE_COUNT * 2.0f - 1.0f;
        const float2 samplePosition = In.Position.xy + direction * t;
        const int2 samplePixel = clamp(int2(samplePosition), int2(0, 0), lastPixel);
        const float3 other = g_Velocity.Load(int3(samplePixel, 0)).xyz;
        const float distance = abs(t) * reach;

        // otherが手前にある: otherのブラーがこのピクセルまで届いていれば、otherの色が乗る。
        const float otherInFront = saturate(1.0f + (self.z - other.z) / SOFT_DEPTH);
        // otherが奥にある: このピクセル自身がブラーで薄くなり、後ろのotherの色が透ける。
        const float otherBehind = saturate(1.0f + (other.z - self.z) / SOFT_DEPTH);
        // 同じ面ではどちらも1になる。足すと輪郭の内側だけ2倍に数えて、輪郭の内外で濃さに段差が出る。
        const float weight = spacing * max(
            otherInFront * BlurCoverage(distance, length(other.xy)),
            otherBehind * BlurCoverage(distance, selfSpeed));

        collected += g_Scene.SampleLevel(g_LinearClamp, samplePosition / ViewportSize, 0.0f).rgb * weight;
        collectedWeight += weight;
    }

    // 周りの色が覆っていない残りの割合は、もとの色のまま。
    const float centerWeight = max(1.0f - collectedWeight, 0.0f);
    const float3 result = (collected + center.rgb * centerWeight) /
        max(collectedWeight + centerWeight, 0.0001f);
    return float4(result, center.a);
}
