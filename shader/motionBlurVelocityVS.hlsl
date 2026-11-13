#include "common.hlsl"
#include "motionBlurCommon.hlsli"

// 骨を持たないもの(手に持つ剣)を、今のフレームと前のフレームのワールド行列で変換する。

VELOCITY_PS_IN main(in VS_IN In)
{
    matrix wvp = mul(mul(World, View), Projection);
    matrix viewProjection = mul(View, Projection);

    VELOCITY_PS_IN Out;
    Out.Position = mul(In.Position, wvp);
    Out.CurrentClip = Out.Position;
    Out.PreviousClip = mul(mul(In.Position, PreviousWorld), viewProjection);
    return Out;
}
