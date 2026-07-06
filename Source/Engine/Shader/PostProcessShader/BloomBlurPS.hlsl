Texture2D SourceTexture : register(t0);
SamplerState LinearSampler : register(s0);

// 定数バッファでぼかし方向を受け取る
cbuffer BlurParams : register(b0)
{
    float2 TexelSize;   // (1 / 画面幅, 1 / 画面高さ)
    float2 padding;
}

float4 Get5x5GaussianBlur(Texture2D<float4> tex, SamplerState smp, float2 uv, float dx, float dy, float4 rect)
{
    float4 ret = tex.Sample(smp,uv);
    float4 blurColor = float4(0, 0, 0, 0);
    float Weights[5][5] =
    {
        { 1 / 273.0, 4 / 273.0, 7 / 273.0, 4 / 273.0, 1 / 273.0 },
        { 4 / 273.0, 16 / 273.0, 26 / 273.0, 16 / 273.0, 4 / 273.0 },
        { 7 / 273.0, 26 / 273.0, 41 / 273.0, 26 / 273.0, 7 / 273.0 },
        { 4 / 273.0, 16 / 273.0, 26 / 273.0, 16 / 273.0, 4 / 273.0 },
        { 1 / 273.0, 4 / 273.0, 7 / 273.0, 4 / 273.0, 1 / 273.0 }
    };
    float offsets[5] = { -2.0f, -1.0f, 0.0, 1.0f, 2.0f };
    for (int i = 0; i < 5; ++i)
    {
        for (int j = 0; j < 5; ++j)
        {
            float2 offset = float2(offsets[i] * dx, offsets[j] * dy);
            float2 sampleUV = uv + offset;
            sampleUV.x = clamp(sampleUV.x, rect.x + dx * 0.5, rect.z - dx * 0.5);
            sampleUV.y = clamp(sampleUV.y, rect.y + dy * 0.5, rect.w - dy * 0.5);
            blurColor += tex.Sample(smp, sampleUV) * Weights[i][j];

        }

    }
    
    return float4(blurColor.rgb, ret.a);

}

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{  
    // 画面全体が対象なので、rectは0,0,1,1固定
    return Get5x5GaussianBlur(SourceTexture, LinearSampler, uv, TexelSize.x, TexelSize.y, float4(0, 0, 1, 1));

}
