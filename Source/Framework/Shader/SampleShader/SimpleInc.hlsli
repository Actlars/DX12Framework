Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

struct VSInput
{
    float3 Position : POSITION; // 頂点座標
    float3 Normal   : NORMAL;   // 法線ベクトル
    float2 TexCoord : TEXCOORD; // テクスチャ座標
    float3 Tangent  : TANGENT;  // 接線ベクトル
};

struct VSOutput
{
    float4 Position : SV_POSITION;  // 頂点座標
    float2 TexCoord : TEXCOORD0;
    //float4 Color    : COLOR;        // 頂点カラー  
};

struct PSOutput
{
    float4 Color : SV_TARGET0;  // ピクセルカラー
};

cbuffer Transform : register(b0)
{
    float4x4 World : packoffset(c0);    // ワールド行列
    float4x4 View  : packoffset(c4);    // ビュー行列
    float4x4 Proj  : packoffset(c8);    // 射影行列
}
