#include "common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

float4 main(in PS_IN In) : SV_Target
{
    float4 outDiffuse;
    if (Material.TextureEnable)
    {
        outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
        outDiffuse *= In.Diffuse;
    }
    else
    {
        outDiffuse = In.Diffuse;
    }

    // The paladin uses very dark plate textures.  A small post-texture lift
    // keeps the armor silhouette and material details visible in gameplay,
    // without flattening the directional shading in the vertex shader.
    outDiffuse.rgb = saturate(outDiffuse.rgb + float3(0.08f, 0.08f, 0.08f));
    outDiffuse.a = In.Diffuse.a;
    return outDiffuse;
}
