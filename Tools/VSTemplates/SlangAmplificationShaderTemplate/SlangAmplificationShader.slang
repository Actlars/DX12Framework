struct Payload
{
    uint MeshletIndex;
};

groupshared Payload s_Payload;

[shader("amplification")]
[numthreads(1, 1, 1)]
void main(uint3 groupID : SV_GroupID)
{
    s_Payload.MeshletIndex = groupID.x;

    // 起動するMesh Shaderのスレッドグループ数（この例では1個だけ）
    DispatchMesh(1, 1, 1, s_Payload);
}