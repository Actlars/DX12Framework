// MeshShaderPS.hlsl
#include "StandardMeshShader.hlsli"

cbuffer MaterialBuffer : register(b1)
{
    float3 Diffuse : packoffset(c0);
    float Alpha : packoffset(c0.w);
    float3 Specular : packoffset(c1);
    float Shininess : packoffset(c1.w);
    float3 Emissive : packoffset(c2);
    float Padding : packoffset(c2.w);
}

Texture2D DiffuseTexture : register(t0);
SamplerState LinearSampler : register(s0);

// Bindless
cbuffer MaterialIndices : register(b1)
{
    uint DiffuseTextureIndex;
}

float4 main(VSOutput input) : SV_TARGET
{
    Texture2D diffuseTex = ResourceDescriptorHeap[DiffuseTextureIndex];
    float4 color = DiffuseTexture.Sample(LinearSampler, input.TexCoord);

    // テクスチャのアルファが低い部分は描かない（穴を開ける）
    clip(color.a - 0.5);

    return float4(color.rgb, 1.0);
}
