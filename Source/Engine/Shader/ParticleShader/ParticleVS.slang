// -------------------------------------------------------------------------------
// ParticleVS.slang
//
// 概要 :
//  生存リストを1インスタンス1粒として読み、カメラへ正対する四角形を組み立てる
//
//  頂点バッファもインデックスバッファも持たない
//  SV_VertexID から角の位置を求めるため、
//  CPUからGPUへ送るのは定数バッファだけで済む
// -------------------------------------------------------------------------------
#include "ParticleCommon.hlsli"

// -------------------------------------------------------------------------------
// 描画用の定数（CPU側 ParticleViewParams と同じ並び）
// 発生・更新用とはレジスタが競合しないよう、別のルートシグネチャで b0 に置く
// -------------------------------------------------------------------------------
cbuffer ParticleViewParams : register(b0)
{
    float4x4 g_ViewProjection;

    float3   g_CameraRight;
    float    g_StartSize;

    float3   g_CameraUp;
    float    g_EndSize;

    float4   g_StartColor;
    float4   g_EndColor;
};

StructuredBuffer<GpuParticle> g_Particles : register(t0);
StructuredBuffer<uint>        g_AliveList : register(t1);

struct VSOutput
{
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR0;
};

[shader("vertex")]
VSOutput main(uint _vertexID : SV_VertexID, uint _instanceID : SV_InstanceID)
{
    VSOutput output;

    const uint particleIndex = g_AliveList[_instanceID];
    const GpuParticle particle = g_Particles[particleIndex];

    // 寿命に対する進み具合(0.0 = 生成直後, 1.0 = 消滅直前)
    const float t = saturate(particle.Age / max(particle.LifeTime, 0.0001f));

    // -------------------------------------------------------------------------------
    // 四角形の角を SV_VertexID から求める
    //   0,1,2 / 0,2,3 の並びで三角形2枚ぶんの6頂点を作る
    // -------------------------------------------------------------------------------
    const uint  cornerTable[6] = { 0, 1, 2, 0, 2, 3 };
    const uint  corner = cornerTable[_vertexID];

    // 角の並び : 左上(-1,+1) 右上(+1,+1) 右下(+1,-1) 左下(-1,-1)
    const float2 offsets[4] =
    {
        float2(-1.0f,  1.0f),
        float2( 1.0f,  1.0f),
        float2( 1.0f, -1.0f),
        float2(-1.0f, -1.0f),
    };

    float2 offset = offsets[corner];

    // 粒ごとの回転。同じ絵が並んで見えるのを防ぐ
    const float sinR = sin(particle.Rotation);
    const float cosR = cos(particle.Rotation);
    offset = float2(offset.x * cosR - offset.y * sinR,
                    offset.x * sinR + offset.y * cosR);

    const float size = lerp(g_StartSize, g_EndSize, t) * 0.5f;

    // カメラの右方向と上方向へ広げることで、常にカメラへ正対させる
    const float3 worldPosition =
        particle.Position +
        g_CameraRight * (offset.x * size) +
        g_CameraUp    * (offset.y * size);

    output.Position = mul(float4(worldPosition, 1.0f), g_ViewProjection);

    // UVは0～1。ピクセルシェーダーで中心からの距離を求めるのに使う
    output.UV    = offsets[corner] * 0.5f + 0.5f;
    output.Color = lerp(g_StartColor, g_EndColor, t);

    return output;
}
