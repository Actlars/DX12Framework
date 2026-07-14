ByteAddressBuffer inputBuffer1  : register(t0);
ByteAddressBuffer inputBuffer2  : register(t1);
RWStructuredBuffer<int32_t> outputBuffer : register(u0);

[shader("compute")]
[numthreads(4, 4, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // numthreads(4,4,1) なので、x,yの2次元インデックスを
    // 0～15の1次元インデックスに変換する
    uint threadID = dispatchThreadID.y * 4 + dispatchThreadID.x;

    CoopVec<int32_t, 32> lhs = coopVecLoad<32, int32_t>(inputBuffer1);
    CoopVec<int32_t, 32> rhs = coopVecLoad<32, int32_t>(inputBuffer2);
    let result = lhs + rhs;

    for (int i = 0; i < result.getCount(); ++i)
        outputBuffer[threadID * 32 + i] = result[i];
}