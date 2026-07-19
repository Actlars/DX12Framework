// -------------------------------------------------------------------------------
// CoopVecMatMulTest.slang
//
// 目的:
//   coopVecMatMul（行列×ベクトル演算）が、この環境で正しく動作するかを
//   最小構成（4x4行列 × 4要素ベクトル）で検証する。
//
//   これは「ニューラルネットワークの全結合層1つ分」に相当する計算。
//   足し算の検証(CoopVecTest.slang)の次の段階として、
//   「行列を掛けて次元を変換する」という本来のニューラル推論の核を確認する。
// -------------------------------------------------------------------------------

// 入力ベクトル（4要素のint32_t）
ByteAddressBuffer inputVector : register(t0);

// 重み行列（4行×4列、行優先(RowMajor)でメモリ上に並んでいる）
ByteAddressBuffer weightMatrix : register(t1);

// 出力結果（4要素のint32_t、1スレッド分のみ）
RWStructuredBuffer<int32_t> outputBuffer : register(u0);

[shader("compute")]
[numthreads(1, 1, 1)]
void main()
{
    // 4要素の入力ベクトルを読み込む
    CoopVec<int32_t, 4> vec = coopVecLoad<4, int32_t>(inputVector);

    // 4x4行列を掛ける。テンプレート引数<出力型, 出力次元M, 入力次元K>
    let result = coopVecMatMul<int32_t, 4, 4>(
        vec,                                // 入力ベクトル
        CoopVecComponentType::SignedInt32,  // ベクトル側の解釈
        weightMatrix,                       // 行列データ
        0,                                   // 行列のバイトオフセット
        CoopVecComponentType::SignedInt32,  // 行列側の解釈
        CoopVecMatrixLayout::RowMajor,      // 行優先で格納されている
        false,                               // 転置しない
        4 * sizeof(int32_t));               // 1行のバイトストライド(4要素分)

    for (int i = 0; i < result.getCount(); ++i)
        outputBuffer[i] = result[i];
}
