// -------------------------------------------------------------------------------
// NTCImageDecode.slang (2階層潜在グリッド対応版)
//
// 目的:
//   高解像度グリッド(細部用)と低解像度グリッド(大域構造用)の2つを
//   サンプリングして繋げ(concat)、MLPでデコードする。
//   これによりPythonのtwo_level_test.pyと完全に対応する構造になる。
// -------------------------------------------------------------------------------

ByteAddressBuffer latentGridHigh : register(t0); // [GRID_HIGH_H][GRID_HIGH_W][LATENT_DIM_HIGH]
ByteAddressBuffer latentGridLow  : register(t1); // [GRID_LOW_H][GRID_LOW_W][LATENT_DIM_LOW]
ByteAddressBuffer weightBuffer   : register(t2); // Layer0/Layer1 重み+バイアス連結
RWTexture2D<float4> outputTexture : register(u0);

// ↓ Python側の学習時に表示された値と完全に一致させること
static const uint IMAGE_W = 128;
static const uint IMAGE_H = 64;

static const uint GRID_HIGH_W = 64;
static const uint GRID_HIGH_H = 32;
static const uint GRID_LOW_W  = 16;
static const uint GRID_LOW_H  = 8;

static const uint LATENT_DIM_HIGH = 4;
static const uint LATENT_DIM_LOW  = 8;
static const uint COMBINED_DIM    = LATENT_DIM_HIGH + LATENT_DIM_LOW; // MLP入力次元

static const uint HIDDEN_DIM = 32;
static const uint OUTPUT_DIM = 3;
static const uint MAX_DIM    = 32; // 中間配列サイズ。HIDDEN_DIM以上であればOK

static const uint LAYER0_WEIGHT_OFFSET = 0;
static const uint LAYER0_BIAS_OFFSET   = LAYER0_WEIGHT_OFFSET + (COMBINED_DIM * HIDDEN_DIM) * 4;
static const uint LAYER1_WEIGHT_OFFSET = LAYER0_BIAS_OFFSET + (HIDDEN_DIM * 4);
static const uint LAYER1_BIAS_OFFSET   = LAYER1_WEIGHT_OFFSET + (HIDDEN_DIM * OUTPUT_DIM) * 4;

// 汎用: 指定したグリッドバッファの(gx, gy)マスのchチャンネル目を1つ読む
float LoadLatent(ByteAddressBuffer buf, uint gridW, uint gx, uint gy, uint dim, uint ch)
{
    uint index = (gy * gridW + gx) * dim + ch;
    return asfloat(buf.Load(index * 4));
}

// 汎用バイリニアサンプリング(高解像度・低解像度どちらのグリッドにも使う)
void SampleGridBilinear(
    ByteAddressBuffer buf, uint gridW, uint gridH, uint dim,
    float2 uv, out float outVec[MAX_DIM])
{
    float gx = uv.x * (gridW - 1);
    float gy = uv.y * (gridH - 1);

    uint x0 = uint(clamp(floor(gx), 0.0f, float(gridW - 2)));
    uint y0 = uint(clamp(floor(gy), 0.0f, float(gridH - 2)));
    uint x1 = x0 + 1;
    uint y1 = y0 + 1;

    float fx = gx - float(x0);
    float fy = gy - float(y0);

    [unroll]
    for (uint c = 0; c < dim; ++c)
    {
        float v00 = LoadLatent(buf, gridW, x0, y0, dim, c);
        float v10 = LoadLatent(buf, gridW, x1, y0, dim, c);
        float v01 = LoadLatent(buf, gridW, x0, y1, dim, c);
        float v11 = LoadLatent(buf, gridW, x1, y1, dim, c);

        float v0 = lerp(v00, v10, fx);
        float v1 = lerp(v01, v11, fx);
        outVec[c] = lerp(v0, v1, fy);
    }
}

// 全結合レイヤー: y = activation(W * x + b)
void FCLayer(
    in  float inputVec[MAX_DIM],
    out float outputVec[MAX_DIM],
    uint inDim, uint outDim,
    uint weightByteOffset, uint biasByteOffset,
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

float Sigmoid(float x)
{
    return 1.0f / (1.0f + exp(-x));
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint px = dispatchThreadID.x;
    uint py = dispatchThreadID.y;

    if (px >= IMAGE_W || py >= IMAGE_H)
        return;

    float2 uv = float2((float(px) + 0.5f) / float(IMAGE_W), (float(py) + 0.5f) / float(IMAGE_H));

    // 高解像度・低解像度、それぞれ別々にサンプリング
    float featHigh[MAX_DIM];
    SampleGridBilinear(latentGridHigh, GRID_HIGH_W, GRID_HIGH_H, LATENT_DIM_HIGH, uv, featHigh);

    float featLow[MAX_DIM];
    SampleGridBilinear(latentGridLow, GRID_LOW_W, GRID_LOW_H, LATENT_DIM_LOW, uv, featLow);

    // 2つの特徴を繋げる(concat) — Pythonの torch.cat([feat_high, feat_low]) と同じ
    float combined[MAX_DIM];
    [unroll]
    for (uint i = 0; i < LATENT_DIM_HIGH; ++i)
        combined[i] = featHigh[i];
    [unroll]
    for (uint i = 0; i < LATENT_DIM_LOW; ++i)
        combined[LATENT_DIM_HIGH + i] = featLow[i];

    float hidden[MAX_DIM];
    FCLayer(combined, hidden, COMBINED_DIM, HIDDEN_DIM, LAYER0_WEIGHT_OFFSET, LAYER0_BIAS_OFFSET, true);

    float outVals[MAX_DIM];
    FCLayer(hidden, outVals, HIDDEN_DIM, OUTPUT_DIM, LAYER1_WEIGHT_OFFSET, LAYER1_BIAS_OFFSET, false);

    float3 rgb = float3(Sigmoid(outVals[0]), Sigmoid(outVals[1]), Sigmoid(outVals[2]));

    outputTexture[uint2(px, py)] = float4(rgb, 1.0f);
}