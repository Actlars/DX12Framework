// MeshShaderPS.hlsl
#include "StandardMeshShader.hlsli"

// Bindless
cbuffer MaterialIndices : register(b1)
{
    uint DiffuseTextureIndex;
    uint NormalTextureIndex;
    uint SpecularTextureIndex;
    uint _Padding0;
    uint _Padding1;
    uint _Padding2;
}

SamplerState g_Sampler : register(s0);

float4 main(VSOutput input) : SV_TARGET
{
    Texture2D diffuseTex = ResourceDescriptorHeap[DiffuseTextureIndex];
    float4 color = diffuseTex.Sample(g_Sampler, input.TexCoord);

    // テクスチャのアルファが低い部分は描かない（穴を開ける）
    clip(color.a - 0.5);

    return float4(color.rgb, 1.0);
}
