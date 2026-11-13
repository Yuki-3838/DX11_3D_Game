// キャラクターのモーションブラーで共有する定数と構造体。
// C++側の対応: system/MotionBlur.cpp の MotionBlurParams。

// 前のフレームの骨の行列。頂点シェーダーで「前のフレームの位置」を求めるのに使う。
// MAX_BONE は common.hlsl で定義している(このバッファを使う頂点シェーダーだけが宣言する)。
#ifdef MAX_BONE
cbuffer MotionBlurPreviousBones : register(b9)
{
    matrix PreviousBoneMatrix[MAX_BONE];
};
#endif

cbuffer MotionBlurParams : register(b10)
{
    // 前のフレームのワールド行列(カメラの追従の分は打ち消し済み)。
    matrix PreviousWorld;
    float2 ViewportSize;
    // フレーム間の移動量のうち、ブラーの片側の長さにする割合。
    float BlurStrength;
    // ブラーの片側の長さの上限(ピクセル)。
    float MaxBlurRadius;
    int TileSize;
    int DebugView;
    int2 ParamsPadding;
};

struct VELOCITY_PS_IN
{
    float4 Position : SV_POSITION;
    float4 CurrentClip : TEXCOORD0;
    float4 PreviousClip : TEXCOORD1;
};
