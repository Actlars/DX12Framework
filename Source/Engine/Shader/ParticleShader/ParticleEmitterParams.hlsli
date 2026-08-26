// -------------------------------------------------------------------------------
// ParticleEmitterParams.hlsli
//
// 概要 :
//  発生と更新のコンピュートシェーダーが読む定数バッファ
//
//  描画側（ParticleVS）も b0 を使うため、共通ヘッダには置かない
//  必要な側だけがこのファイルを取り込む
// -------------------------------------------------------------------------------
#ifndef PARTICLE_EMITTER_PARAMS_HLSLI
#define PARTICLE_EMITTER_PARAMS_HLSLI

// -------------------------------------------------------------------------------
// 発生・更新用のパラメータ（CPU側 EmitterParams と同じ並び）
// -------------------------------------------------------------------------------
cbuffer EmitterParams : register(b0)
{
    float3  g_Origin;
    float   g_DeltaTime;

    float3  g_Gravity;
    float   g_Drag;

    float   g_LifeTime;
    float   g_LifeTimeRandom;
    float   g_InitialSpeed;
    float   g_InitialSpeedRandom;

    float   g_SpreadAngle;
    float   g_ShapeRadius;
    uint    g_Shape;
    uint    g_MaxParticles;

    uint    g_SpawnCount;
    uint    g_RandomSeed;
    uint    g_EmitterPad0;
    uint    g_EmitterPad1;
};

#endif // PARTICLE_EMITTER_PARAMS_HLSLI
