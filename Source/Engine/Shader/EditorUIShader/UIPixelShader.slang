struct PSInput
{
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR;
};

Texture2D       g_MainTexture   : register(t0);
SamplerState    g_LinearSampler : register(s0);

[shader("fragment")]
float4 main(PSInput input) : SV_TARGET
{
    float4 texColor = g_MainTexture.Sample(g_LinearSampler, input.UV);
    return texColor * input.Color;
}