#include "common.hlsl"

[maxvertexcount(6)]
void main(line PS_IN input[2], inout TriangleStream<PS_IN> OutputStream)
{
    PS_IN output;
    // Old callers use 2-3 as a pixel-like width. New callers pass an NDC width.
    float thickness = LineWidth > 0.1f ? LineWidth * 0.0015f : LineWidth;

    // Work in NDC. Offsetting raw clip coordinates made distant lines collapse
    // into dots and nearby lines grow into giant bars.
    float w0 = max(abs(input[0].Position.w), 0.00001f);
    float w1 = max(abs(input[1].Position.w), 0.00001f);
    float2 p0 = input[0].Position.xy / w0;
    float2 p1 = input[1].Position.xy / w1;
    float2 delta = p1 - p0;
    float deltaLength = max(length(delta), 0.00001f);
    float2 dir = delta / deltaLength;

    // 正規化された方向に直交するベクトルを計算（線の幅のため）
    float2 normal = float2(-dir.y, dir.x) * thickness;

    // 四角形の頂点を計算（2つの三角形を形成）
    float4 pos1 = input[0].Position + float4(normal * w0, 0.0, 0.0);
    float4 pos2 = input[0].Position - float4(normal * w0, 0.0, 0.0);
    float4 pos3 = input[1].Position + float4(normal * w1, 0.0, 0.0);
    float4 pos4 = input[1].Position - float4(normal * w1, 0.0, 0.0);

    // 最初の三角形
    output.Position = pos1;
    output.Diffuse = input[0].Diffuse;
    output.TexCoord.xy = 0.0f;
    OutputStream.Append(output);

    output.Position = pos2;
    output.Diffuse = input[0].Diffuse;
    output.TexCoord.xy = 0.0f;
    OutputStream.Append(output);
    
    output.Position = pos3;
    output.Diffuse = input[0].Diffuse;
    output.TexCoord.xy = 0.0f;
    OutputStream.Append(output);

    // 二番目の三角形
    output.Position = pos2;
    output.Diffuse = input[1].Diffuse;
    output.TexCoord.xy = 0.0f;
    OutputStream.Append(output);
    
    output.Position = pos4;
    output.Diffuse = input[1].Diffuse;
    output.TexCoord.xy = 0.0f;
    OutputStream.Append(output);

    output.Position = pos3;
    output.Diffuse = input[1].Diffuse;
    output.TexCoord.xy = 0.0f;
    OutputStream.Append(output);
    
    OutputStream.RestartStrip();
}
