// 影生成パス用のピクセルシェーダー。
// 深度バッファへの書き込みだけが目的なので、色は何も出力しない。
// レンダーターゲットを割り当てずに描画するため、この戻り値は使われないが、
// このプロジェクトのCShaderは頂点・ピクセルの組で生成するため用意している。

struct SHADOW_PS_IN
{
    float4 Position : SV_POSITION;
};

float4 main(in SHADOW_PS_IN In) : SV_Target
{
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}
