struct PSInput
{
    float4 Position : SV_Position;
};

[shader("fragment")]
float4 main(PSInput input) : SV_TARGET
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}