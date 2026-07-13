struct VSOutput
{
    float4 Position : SV_Position;
    float3 Color    : COLOR0;
};

[shader("vertex")]
VSOutput VSMain(uint vertexId : SV_VertexID)
{
    float2 positions[3] =
    {
        float2( 0.0f,  0.5f),
        float2( 0.5f, -0.5f),
        float2(-0.5f, -0.5f)
    };

    float3 colors[3] =
    {
        float3(1.0f, 0.0f, 0.0f),
        float3(0.0f, 1.0f, 0.0f),
        float3(0.0f, 0.0f, 1.0f)
    };

    VSOutput output;
    output.Position = float4(positions[vertexId], 0.0f, 1.0f);
    output.Color = colors[vertexId];

    return output;
}

[shader("pixel")]
float4 PSMain(VSOutput input) : SV_Target0
{
    return float4(input.Color, 1.0f);
}
