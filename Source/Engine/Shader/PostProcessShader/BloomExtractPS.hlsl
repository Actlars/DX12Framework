Texture2D SourceTexture : register(t0);
SamplerState LinearSampler : register(s0);

float4 main( float4 pos : SV_POSITION, float2 uv : TEXCOORD ) : SV_TARGET
{
    float3 color = SourceTexture.Sample(LinearSampler, uv).rgb;
    float brightness = dot(color, float3(0.299, 0.587, 0.114));
    
    // 明るさが閾値を超えた部分だけを残す
    return brightness > 0.9 ? float4(color, 1.0f) : float4(0, 0, 0, 1);
}
