// -------------------------------------------------------------------------------
// NTCPreviewPS.hlsl
//
// 目的:
//   ベイクしたNTCデコード結果テクスチャを、画面全体にそのまま表示するだけの
//   確認用ピクセルシェーダー。FullScreenVS.hlslとセットで使う。
// -------------------------------------------------------------------------------

Texture2D    g_NTCTexture : register(t0);
SamplerState g_Sampler    : register(s0);

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

[shader("fragment")]
float4 main(PSInput input) : SV_TARGET
{
    return g_NTCTexture.Sample(g_Sampler, input.UV);
}
