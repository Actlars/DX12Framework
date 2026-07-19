// -------------------------------------------------------------------------------
// MLPDecodeTest.hlsl
//
// 目的:
//   coopVecMatMul 等のハードウェア支援API(Cooperative Vector)を一切使わず、
//   通常のHLSL(素のFMAループ)だけで、固定重みの小さな2層MLPが
//   正しく推論できるかを検証する。
//
//   NTC(Neural Texture Compression)実装における
//   「ソフトウェアフォールバックパス」の最小プロトタイプに相当する。
//
//   構成: 潜在ベクトル4要素 → 隠れ層4要素(ReLU) → 出力3要素(線形)
//
//   期待値(手計算):
//     入力          = [1, 2, 3, 4]
//     隠れ層(単位行列 + ReLU) = [1, 2, 3, 4]  ※恒等変換なので入力と同じ
//     出力(合計パターン)      = [10, 6, 4]
//       out[0] = in[0]+in[1]+in[2]+in[3] = 1+2+3+4 = 10
//       out[1] = in[1]+in[3]             = 2+4     = 6
//       out[2] = in[0]+in[2]             = 1+3     = 4
// -------------------------------------------------------------------------------

// 入力潜在ベクトル(4要素のfloat、16バイト)
ByteAddressBuffer inputVector : register(t0);

// 重みバッファ: 全レイヤーの重み+バイアスを連結して格納(RowMajor)
//   Layer0 weight (4x4) : 16 floats  = 64 bytes  [offset   0]
//   Layer0 bias   (4)   :  4 floats  = 16 bytes  [offset  64]
//   Layer1 weight (3x4) : 12 floats  = 48 bytes  [offset  80]
//   Layer1 bias   (3)   :  3 floats  = 12 bytes  [offset 128]
ByteAddressBuffer weightBuffer : register(t1);

// 出力結果(3要素のfloat)
RWStructuredBuffer<float> outputBuffer : register(u0);

#define INPUT_DIM  4
#define HIDDEN_DIM 4
#define OUTPUT_DIM 3
#define MAX_DIM    4 // 中間配列の確保サイズ(このテストでは全レイヤーが4以下)

static const uint LAYER0_WEIGHT_OFFSET = 0;
static const uint LAYER0_BIAS_OFFSET   = LAYER0_WEIGHT_OFFSET + (INPUT_DIM * HIDDEN_DIM) * 4;   // 64
static const uint LAYER1_WEIGHT_OFFSET = LAYER0_BIAS_OFFSET + (HIDDEN_DIM * 4);                  // 80
static const uint LAYER1_BIAS_OFFSET   = LAYER1_WEIGHT_OFFSET + (HIDDEN_DIM * OUTPUT_DIM) * 4;   // 128

// 1層分の全結合レイヤー: y = activation(W * x + b)
// W は RowMajor: W[o][i] が weightBuffer 上で (o * inDim + i) の位置に並んでいる想定
void FCLayer(
    in  float inputVec[MAX_DIM],
    out float outputVec[MAX_DIM],
    uint inDim,
    uint outDim,
    uint weightByteOffset,
    uint biasByteOffset,
    bool applyReLU)
{
    for (uint o = 0; o < outDim; ++o)
    {
        float sum = asfloat(weightBuffer.Load(biasByteOffset + o * 4));
        for (uint i = 0; i < inDim; ++i)
        {
            float w = asfloat(weightBuffer.Load(weightByteOffset + (o * inDim + i) * 4));
            sum += inputVec[i] * w;
        }
        outputVec[o] = applyReLU ? max(sum, 0.0f) : sum;
    }
}

[numthreads(1, 1, 1)]
void main()
{
    // 入力ベクトルを読み込む
    float x[MAX_DIM];
    [unroll]
    for (uint i = 0; i < INPUT_DIM; ++i)
        x[i] = asfloat(inputVector.Load(i * 4));

    // Layer0: 4 -> 4, ReLU
    float h[MAX_DIM];
    FCLayer(x, h, INPUT_DIM, HIDDEN_DIM, LAYER0_WEIGHT_OFFSET, LAYER0_BIAS_OFFSET, true);

    // Layer1: 4 -> 3, 活性化なし(線形出力)
    float y[MAX_DIM];
    FCLayer(h, y, HIDDEN_DIM, OUTPUT_DIM, LAYER1_WEIGHT_OFFSET, LAYER1_BIAS_OFFSET, false);

    [unroll]
    for (uint o = 0; o < OUTPUT_DIM; ++o)
        outputBuffer[o] = y[o];
}
