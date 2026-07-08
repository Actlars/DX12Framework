Texture2D SourceTexture : register(t0);
Texture2D BloomTexture : register(t1);
SamplerState LinearSampler : register(s0);

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    float3 sceneColor = SourceTexture.Sample(LinearSampler, uv).rgb;
    float3 bloomColor = BloomTexture.Sample(LinearSampler, uv).rgb;
    
    // 元の絵に、明るい部分を加算する
    float3 finalColor = sceneColor + bloomColor;
    
    return float4(finalColor, 1.0f);
}
