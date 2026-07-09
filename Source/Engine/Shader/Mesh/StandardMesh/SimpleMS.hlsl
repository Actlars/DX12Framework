// -------------------------------------------------------------------------------
// MSInput structure
// -------------------------------------------------------------------------------
struct MSInput
{
    float3 Position;    // 頂点座標
    float4 Color;       // 頂点カラー
};

// -------------------------------------------------------------------------------
// MSOutput structure
// -------------------------------------------------------------------------------
struct MSOutput
{
    float4 Position : SV_Position;  // 頂点座標
    float4 Color    : COLOR;        // 頂点カラー
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
StructuredBuffer<MSInput>       Vertices    : register(t0);
StructuredBuffer<uint3>         Indices     : register(t1);
ConstantBuffer<TransformParam>  Transform   : register(b0);

// -------------------------------------------------------------------------------
//      メッシュシェーダーのエントリーポイント
// -------------------------------------------------------------------------------
[numthreads(64,1,1)]
[outputtopology("triangle")]
void main
(
    uint groupIndex : SV_GroupIndex,
    out vertices MSOutput verts[3],
    out indices uint3 tris[1]
)
{
    // スレッドグループの頂点とプリミティブの数を設定
    SetMeshOutputCounts(3, 1);
    
    // 頂点番号を設定
    if (groupIndex < 1)
    {
        tris[groupIndex] = Indices[groupIndex];
    }
    
    // 頂点データを設定
    if (groupIndex < 3)
    {
        MSOutput output = (MSOutput) 0;
        
        float4 localPos = float4(Vertices[groupIndex].Position, 1.0f);
        float4 worldPos = mul(Transform.World, localPos);
        float4 viewPos = mul(Transform.View, worldPos);
        float4 projPos = mul(Transform.Proj, viewPos);
        
        output.Position = projPos;
        output.Color = Vertices[groupIndex].Color;
        
        verts[groupIndex] = output;
    }

}