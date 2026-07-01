// MeshShaderPS.hlsl
#include "MeshShaderInc.hlsli"

cbuffer MaterialBuffer : register(b1) // b1 に変更
{
    float3 Diffuse : packoffset(c0);
    float Alpha : packoffset(c0.w);
    float3 Specular : packoffset(c1);
    float Shininess : packoffset(c1.w);
    float3 Emissive : packoffset(c2);
    float Padding : packoffset(c2.w);
}

Texture2D ColorMap : register(t0);
SamplerState WrapSmp : register(s0);

PSOutput main(VSOutput input)
{
    PSOutput output = (PSOutput) 0;

    float4 color = ColorMap.Sample(WrapSmp, input.TexCoord);

    // テクスチャのアルファが低い部分は描かない（穴を開ける）
    clip(color.a - 0.5);

    output.Color = float4(color.rgb, 1.0);
    return output;
}
