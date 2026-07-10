struct PSInput
{
    float4 Position : SV_Position;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD;
    float3 MeshletColor : COLOR1;
};

// RootConstants
cbuffer TextureIndexCB : register(b1){ uint DiffuseTextureIndex; }
cbuffer DebugModeCB : register(b2){ uint DebugMode; }

SamplerState g_Sampler : register(s0);

float4 main(PSInput input) : SV_TARGET
{    
    if (DebugMode == 1)
    {
        return float4(input.MeshletColor, 1.0f);

    }
    
    // Bindless : ヒープから直接テクスチャを取得
    Texture2D diffuseTex = ResourceDescriptorHeap[DiffuseTextureIndex];
    float3 texColor = diffuseTex.Sample(g_Sampler, input.TexCoord).rgb;
    
    float3 LightDir = normalize(float3(0.5f, 1.0f, -0.5f));
    float3 n = normalize(input.Normal);
    float ndotl = saturate(dot(n, LightDir));
    
    float3 color = texColor * (0.2f + 0.8f * ndotl);
    return float4(texColor, 1.0f);
}