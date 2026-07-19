struct MSOutput
{
    float4 Position : SV_Position;
};

[shader("mesh")]
[numthreads(1, 1, 1)]
[outputtopology("triangle")]
void main(
    uint groupIndex : SV_GroupIndex,
    out vertices MSOutput verts[3],
    out indices uint3 tris[1])
{
    SetMeshOutputCounts(3, 1);

    if (groupIndex == 0)
    {
        tris[0] = uint3(0, 1, 2);

        verts[0].Position = float4(-0.5f, -0.5f, 0.0f, 1.0f);
        verts[1].Position = float4( 0.0f,  0.5f, 0.0f, 1.0f);
        verts[2].Position = float4( 0.5f, -0.5f, 0.0f, 1.0f);
    }
}