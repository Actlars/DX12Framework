struct PSInput
{
    float4 Position     : SV_Position;
    float3 Normal       : NORMAL;
    float2 TexCoord     : TEXCOORD;
    float3 MeshletColor : COLOR1;
};

// RootConstants
cbuffer TextureIndexCB : register(b1) { uint DiffuseTextureIndex; }
cbuffer DebugModeCB    : register(b2) { uint DebugMode; }

SamplerState g_Sampler : register(s0);

// -------------------------------------------------------------------------------
//      Bindlessヒープから直接Texture2Dを取得するヘルパー
//      HLSL/DXILターゲット時、リテラルで ResourceDescriptorHeap[index] を生成する
// -------------------------------------------------------------------------------
Texture2D GetBindlessTexture2D(uint index)
{
    __target_switch
    {
    case hlsl:
        __intrinsic_asm "ResourceDescriptorHeap[$0]";
    }
}

[shader("fragment")]
float4 main(PSInput input) : SV_TARGET
{
    if (DebugMode == 1)
    {
        return float4(input.MeshletColor, 1.0f);
    }

    // Bindless : ヒープから直接テクスチャを取得
    Texture2D diffuseTex = GetBindlessTexture2D(DiffuseTextureIndex);
    float3 texColor = diffuseTex.Sample(g_Sampler, input.TexCoord).rgb;

    float3 LightDir = normalize(float3(0.5f, 1.0f, -0.5f));
    float3 n = normalize(input.Normal);
    float ndotl = saturate(dot(n, LightDir));

    float3 color = texColor * (0.2f + 0.8f * ndotl);
    return float4(texColor, 1.0f);
}