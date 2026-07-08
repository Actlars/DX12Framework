// 通常のTexture2D宣言(register(t0)等)を一切使わず、
// ResourceDescriptorHeap[]でインデックス経由にリソースへアクセスする

cbuffer MaterialIndices : register(b1)
{
    uint TextureIndex;
}

SamplerState LinearSampler : register(s0); // サンプラーは今回も静的なので変更なし

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    // ここがBindlessの核心：register(t0)のような固定スロットではなく、
    // ヒープ全体からインデックスで直接引く
    Texture2D bindlessTex = ResourceDescriptorHeap[TextureIndex];
    return bindlessTex.Sample(LinearSampler, uv);
}
