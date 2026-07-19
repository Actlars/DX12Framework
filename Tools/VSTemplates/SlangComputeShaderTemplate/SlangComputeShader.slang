RWStructuredBuffer<float> outputBuffer : register(u0);

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    outputBuffer[0] = 0.0f;
}