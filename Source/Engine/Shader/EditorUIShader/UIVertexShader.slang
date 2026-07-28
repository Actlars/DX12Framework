struct VSInput
{
    float2 Position : POSITION;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR0;
};

struct PSInput
{
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR;
}

cbuffer ScreenParam : register(b0)
{
    float2 InvScreenSize;
}

[shader("vertex")]
PSInput main(VSInput input)
{
    PSInput output;

    //  スクリーン座標を-1,1のNDCへ変換する直行射影
    float2 ndc;
    ndc.x = input.Position.x * InvScreenSize.x * 2.0f - 1.0f;
    ndc.y = 1.0 - input.Position.y * InvScreenSize.y * 2.0;

    output.Position = float4(ndc, 0.0, 1.0);
    output.UV       = input.UV;
    output.Color    = input.Color;

    return output;
}