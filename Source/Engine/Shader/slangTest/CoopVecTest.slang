/* CoopVecを用いた足し算プログラム */

// CPUから送られてくる入力データ（各32個のint32_t）
// ByteAddressBufferは生のバイト列として扱うバッファ型
// coopVecLoad関数が、このバイト列をCoopVec型として読み込む
ByteAddressBuffer inputBuffer1  : register(t0);
ByteAddressBuffer inputBuffer2  : register(t1);

/* 計算結果をCPUに返すためのバッファ*/
/* RWStructuredBuffer = GPU側から読み書き可能な構造化バッファ（UAV）*/
RWStructuredBuffer<int32_t> outputBuffer : register(u0);

[shader("compute")]
[numthreads(4, 4, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // SV_DispatchThreadIDは、numthreadsの次元に対応した（x,y,z）の3成分を持つ
    // numthreads(4,4,1)なので、x : 0 ～ 3, y : 0 ～ 3, z : 0 の組み合わせで16通りのスレッドが動く。
    // これを0 ～ 15の一意な番号（このスレッドで自分が何番目か）に変換
    uint threadID = dispatchThreadID.y * 4 + dispatchThreadID.x;

    // CoopVec<int32_t, 32> = [int32_tが32個並んだ、特殊な長さのベクトル」という型
    // CoopVec<32, int32_t>(...)は、ByteAddressBufferから32個のint32_tを
    // まとめて読み込み、CoopVec型として受け取るための関数
    // 全スレッドが同じ32個のデータを読む
    CoopVec<int32_t, 32> lhs = coopVecLoad<32, int32_t>(inputBuffer1);
    CoopVec<int32_t, 32> rhs = coopVecLoad<32, int32_t>(inputBuffer2);

    // CoopVec同士の +演算子。要素ごとの足し算（lhs[0] + rhs[0], lhs[1] + rhs[1],...）を
    // 32個まとめて計算する。これがハードウェア（Tensor Core等）で高速化される想定の演算
    let result = lhs + rhs;

    // 計算結果を、このスレッド専用の領域（32個ずつ）に書き出す
    // 16スレッド分なので、outputBufferは全体で16 * 32 = 512個のint32_tを持つ
    for (int i = 0; i < result.getCount(); ++i)
        outputBuffer[threadID * 32 + i] = result[i];
}
