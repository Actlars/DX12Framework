// -------------------------------------------------------------------------------
// ParticleFinalize.slang
//
// 概要 :
//  1スレッドだけを走らせ、次の2つを行う
//      1. このフレームの生存数から DrawInstancedIndirect の引数を組み立てる
//      2. 生存カウンタを 0 に戻し、次フレームの Simulate に備える
//
//  カウンタをCPUへ読み戻さずに描画数を決められるため、
//  GPUとCPUの同期が一切発生しない
//
//  リセットをここで行えるのは、引数バッファへ値を書き終えた「あと」だから
//  描画はカウンタではなく引数バッファを読むので、
//  この時点で0に戻しても今フレームの描画数には影響しない
// -------------------------------------------------------------------------------
#include "ParticleCommon.hlsli"

RWByteAddressBuffer g_Counters     : register(u3);
RWByteAddressBuffer g_IndirectArgs : register(u4);

[shader("compute")]
[numthreads(1, 1, 1)]
void main()
{
    const uint aliveCount = g_Counters.Load(COUNTER_OFFSET_ALIVE);

    // D3D12_DRAW_ARGUMENTS の並び
    //   VertexCountPerInstance / InstanceCount / StartVertexLocation / StartInstanceLocation
    //
    // ビルボードは三角形2枚 = 6頂点。頂点バッファは持たず、SV_VertexID から組み立てる
    g_IndirectArgs.Store4(0, uint4(6, aliveCount, 0, 0));

    // 次フレームの Simulate は 0 から数え直す
    g_Counters.Store(COUNTER_OFFSET_ALIVE, 0);
}
