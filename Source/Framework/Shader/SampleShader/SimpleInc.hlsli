struct VSInput
{
    float3 Position : POSITION; // 頂点座標
    float4 Color    : COLOR;    // 頂点カラー
};

struct VSOutput
{
    float4 Position : SV_POSITION;  // 頂点座標
    float4 Color    : COLOR;        // 頂点カラー  
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
