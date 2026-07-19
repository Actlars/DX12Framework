// -------------------------------------------------------------------------------
// MSInput structure
// -------------------------------------------------------------------------------
struct MSInput
{
    float3 Position;    // 頂点座標
    float3 Normal;      // 法線
    float2 TexCoord;    // UV
    float3 Tangent;     // タンジェント
    float3 Bitangent;
};

// -------------------------------------------------------------------------------
// MSOutput structure
// -------------------------------------------------------------------------------
struct MSOutput
{
    float4 Position     : SV_Position;  // 頂点座標
    float3 Normal       : NORMAL;
    float2 TexCoord     : TEXCOORD;
    float3 MeshletColor : COLOR1;
};

// -------------------------------------------------------------------------------
// MeshletDesc structure
// -------------------------------------------------------------------------------
struct MeshletDesc
{
    uint VertexOffset;
    uint VertexCount;
    uint PrimitiveOffset;
    uint PrimitiveCount;
};

// -------------------------------------------------------------------------------
// TransformParam structure
// -------------------------------------------------------------------------------
struct TransformParam
{
    float4x4 World; // ワールド行列
    float4x4 View;  // ビュー行列
    float4x4 Proj;  // 射影行列
};

// -------------------------------------------------------------------------------
// Resources
// -------------------------------------------------------------------------------
StructuredBuffer<MSInput>       Vertices                : register(t0);
StructuredBuffer<uint>          MeshletVertexIndices    : register(t1); // ローカル→グローバル頂点インデックス
StructuredBuffer<uint>          PackedPrimitiveIndices  : register(t2); // 3バイトパック済み三角形
StructuredBuffer<MeshletDesc>   Meshlets                : register(t3);
ConstantBuffer<TransformParam>  Transform               : register(b0);

// packedから3頂点分のローカルインデックスを取り出す
uint3 UnpackTriangle(uint packed)
{
    return uint3(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF);
}

// メッシュレットIDから疑似ランダムな色を作る
float3 HashColor(uint id)
{
    uint    h = id * 2654435761u;
    float   r = ((h >> 0) & 0xFF)   / 255.0f;
    float   g = ((h >> 8) & 0xFF)   / 255.0f;
    float   b = ((h >> 16) & 0xFF)  / 255.0f;
    return float3(r, g, b);
}


// -------------------------------------------------------------------------------
//      メッシュシェーダーのエントリーポイント
// -------------------------------------------------------------------------------
[numthreads(128,1,1)]
[outputtopology("triangle")]
void main
(
    uint groupIndex : SV_GroupIndex,
    uint3 groupID : SV_GroupID,
    out vertices MSOutput verts[64],
    out indices uint3 tris[126]
)
{    
    // スレッドグループごとのIDを取得
    MeshletDesc msDesc = Meshlets[groupID.x];
    // スレッドグループの頂点とプリミティブの数を設定
    SetMeshOutputCounts(msDesc.VertexCount, msDesc.PrimitiveCount);
    
    if (groupIndex < msDesc.PrimitiveCount)
    {
        uint packed = PackedPrimitiveIndices[msDesc.PrimitiveOffset + groupIndex];
        tris[groupIndex] = UnpackTriangle(packed); // ローカル頂点座標（0 ～ 63）そのまま使える
    }
    
    if (groupIndex < msDesc.VertexCount)
    {
        // ローカル → グローバル頂点インデックスへ変換してから実データを読む
        uint globalVertexIndex = MeshletVertexIndices[msDesc.VertexOffset + groupIndex];
        MSInput v = Vertices[globalVertexIndex];
        
        MSOutput output = (MSOutput) 0;
        float4 worldPos = mul(Transform.World, float4(v.Position, 1.0f));
        float4 viewPos  = mul(Transform.View, worldPos);
        float4 projPos  = mul(Transform.Proj, viewPos);
        
        output.Position     = projPos;
        output.Normal       = mul((float3x3) Transform.World, v.Normal);
        output.TexCoord     = v.TexCoord;
        output.MeshletColor = HashColor(groupID.x);
        
        verts[groupIndex] = output;
    }
}