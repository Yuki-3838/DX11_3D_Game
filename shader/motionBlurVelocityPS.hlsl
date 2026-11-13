#include "motionBlurCommon.hlsli"

// 速度バッファへ書く。
// rg: ブラーの片側の長さ(ピクセル)。b: カメラからの深度(前後の判定に使う)。

float4 main(VELOCITY_PS_IN In) : SV_Target
{
    const float depth = In.CurrentClip.w;
    // 前のフレームの位置がカメラの後ろなら、画面上の移動量を求められない。
    if (In.PreviousClip.w <= 0.01f)
        return float4(0.0f, 0.0f, depth, 1.0f);

    const float2 current = In.CurrentClip.xy / In.CurrentClip.w;
    const float2 previous = In.PreviousClip.xy / In.PreviousClip.w;
    // クリップ空間はYが上向き、ピクセル座標はYが下向き。
    float2 velocity = (current - previous) * float2(0.5f, -0.5f) * ViewportSize * BlurStrength;

    const float speed = length(velocity);
    if (speed > MaxBlurRadius)
        velocity *= MaxBlurRadius / speed;
    return float4(velocity, depth, 1.0f);
}
