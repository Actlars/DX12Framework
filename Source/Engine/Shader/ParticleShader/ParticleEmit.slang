// -------------------------------------------------------------------------------
// ParticleEmit.slang
//
// 概要 :
//  このフレームに発生させる数だけスレッドを立ち上げ、
//  未使用スロット(DeadList)から1つ取り出して初期化する
//
// Wave intrinsics による最適化 :
//  素直に書くと「1スレッドにつき1回 InterlockedAdd」となり、
//  同じアドレスへの原子加算が大量に直列化してボトルネックになる
//
//  ここではまず波(wave)の中で何スレッドが確保するかを数え、
//  代表の1レーンだけがまとめて原子加算を行う
//  各レーンは波内での自分の順番(WavePrefixCountBits)を足すだけで
//  重複しないスロットを得られる
//
//  結果として、原子加算の回数が 1/波幅（32～64分の1）に減る
// -------------------------------------------------------------------------------
#include "ParticleCommon.hlsli"
#include "ParticleEmitterParams.hlsli"

RWStructuredBuffer<GpuParticle>  g_Particles : register(u0);
RWStructuredBuffer<uint>         g_DeadList  : register(u1);
RWStructuredBuffer<uint>         g_AliveList : register(u2);
RWByteAddressBuffer              g_Counters  : register(u3);

[shader("compute")]
[numthreads(PARTICLE_THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 _dispatchThreadID : SV_DispatchThreadID)
{
    // このスレッドが実際に粒を出すかどうか
    // 発生数はグループサイズの倍数とは限らないため、末尾のスレッドは空回りする
    const bool wantsSpawn = (_dispatchThreadID.x < g_SpawnCount);

    // -------------------------------------------------------------------------------
    // 波の中で「確保したいレーン」を数える
    //  laneCount  : この波で何個必要か
    //  laneOffset : この波の中で自分が何番目か
    // -------------------------------------------------------------------------------
    const uint laneCount  = WaveActiveCountBits(wantsSpawn);
    const uint laneOffset = WavePrefixCountBits(wantsSpawn);

    if (laneCount == 0)
    {
        return; // この波には出すものがない
    }

    // -------------------------------------------------------------------------------
    // 代表レーンが、波ぶんをまとめて DeadList から確保する
    //
    // DeadCount は「残っている未使用スロットの数」
    // 減算前の値が返るので、そこから自分の順番を引けば取り出す位置が決まる
    // -------------------------------------------------------------------------------
    uint deadCountBefore = 0;

    if (WaveIsFirstLane())
    {
        g_Counters.InterlockedAdd(COUNTER_OFFSET_DEAD, (uint)(-(int)laneCount), deadCountBefore);
    }

    // 代表レーンが得た値を波全体へ配る
    deadCountBefore = WaveReadLaneFirst(deadCountBefore);

    if (!wantsSpawn)
    {
        return;
    }

    // 在庫が足りない場合、あふれたレーンは何もしない
    // （減らしすぎたぶんは、次のフレームの Simulate が返却して整う）
    if (laneOffset >= deadCountBefore)
    {
        return;
    }

    // 末尾から取り出す（スタックとして扱う）
    const uint deadIndex     = deadCountBefore - laneOffset - 1;
    const uint particleIndex = g_DeadList[deadIndex];

    if (particleIndex >= g_MaxParticles)
    {
        return; // 念のための境界確認
    }

    // -------------------------------------------------------------------------------
    // 初期化
    // -------------------------------------------------------------------------------
    uint rng = PcgHash(g_RandomSeed + particleIndex * 9781u + _dispatchThreadID.x);

    float3 position = g_Origin;

    if (g_Shape == EMITTER_SHAPE_SPHERE)
    {
        position += RandomDirection(rng) * g_ShapeRadius;
    }
    else if (g_Shape == EMITTER_SHAPE_BOX)
    {
        position += float3(RandomSigned(rng), RandomSigned(rng), RandomSigned(rng)) * g_ShapeRadius;
    }

    // 上向きを軸に、広がり角の内側へ射出する
    const float3 direction = RandomCone(rng, float3(0.0f, 1.0f, 0.0f), g_SpreadAngle * 0.5f);
    const float  speed     = g_InitialSpeed + RandomSigned(rng) * g_InitialSpeedRandom;

    GpuParticle particle;
    particle.Position = position;
    particle.Age      = 0.0f;
    particle.Velocity = direction * speed;

    // 寿命が0以下だと「未使用」と区別できなくなるため、必ず正の値にする
    particle.LifeTime = max(0.01f, g_LifeTime + RandomSigned(rng) * g_LifeTimeRandom);

    particle.Seed     = RandomFloat(rng);
    particle.Rotation = RandomFloat(rng) * 6.28318530718f;
    particle.Pad0     = 0.0f;
    particle.Pad1     = 0.0f;

    g_Particles[particleIndex] = particle;
}
