// -------------------------------------------------------------------------------
// ParticleSimulate.slang
//
// 概要 :
//  プール全体を1スレッド1粒で走査し、
//      生きている  → 位置と速度を進め、描画対象(AliveList)へ登録
//      寿命切れ    → スロットを未使用(DeadList)へ返却
//  を行う
//
// Wave intrinsics による最適化 :
//  AliveList への追加も DeadList への返却も、素直に書くと
//  粒ごとに InterlockedAdd が必要になる
//  プールが数万個あると、この原子加算だけで処理時間が決まってしまう
//
//  そこで波の中で該当するレーン数を数え、代表レーンが1回だけまとめて確保する
//  原子加算の回数は 1/波幅 になり、書き込み位置は
//  波内の順番(WavePrefixCountBits)から重複なく決まる
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
    const uint index    = _dispatchThreadID.x;
    const bool inRange  = (index < g_MaxParticles);

    // 範囲外のレーンも wave 命令には参加させる必要があるため、早期 return しない
    GpuParticle particle = (GpuParticle)0;

    if (inRange)
    {
        particle = g_Particles[index];
    }

    // このスロットが現在使われているか
    bool wasAlive = inRange && (particle.LifeTime > 0.0f);
    bool isAlive  = false;
    bool justDied = false;

    if (wasAlive)
    {
        particle.Age += g_DeltaTime;

        if (particle.Age >= particle.LifeTime)
        {
            // 寿命切れ。スロットを解放する
            particle.LifeTime = 0.0f;
            justDied = true;
        }
        else
        {
            // 減衰は「1秒でDragの割合だけ速度を失う」という素直な近似
            const float damping = max(0.0f, 1.0f - g_Drag * g_DeltaTime);

            particle.Velocity = particle.Velocity * damping + g_Gravity * g_DeltaTime;
            particle.Position += particle.Velocity * g_DeltaTime;

            isAlive = true;
        }

        g_Particles[index] = particle;
    }

    // -------------------------------------------------------------------------------
    // 生きている粒を描画対象へ登録する
    //
    //  波の中の該当数をまとめて確保し、原子加算を1回に減らす
    // -------------------------------------------------------------------------------
    {
        const uint aliveInWave = WaveActiveCountBits(isAlive);
        const uint aliveOffset = WavePrefixCountBits(isAlive);

        uint writeBase = 0;

        if (aliveInWave > 0)
        {
            if (WaveIsFirstLane())
            {
                g_Counters.InterlockedAdd(COUNTER_OFFSET_ALIVE, aliveInWave, writeBase);
            }

            writeBase = WaveReadLaneFirst(writeBase);

            if (isAlive)
            {
                g_AliveList[writeBase + aliveOffset] = index;
            }
        }
    }

    // -------------------------------------------------------------------------------
    // 寿命切れのスロットを未使用へ返却する
    //
    //  こちらも同じ手順でまとめて確保する
    // -------------------------------------------------------------------------------
    {
        const uint deadInWave = WaveActiveCountBits(justDied);
        const uint deadOffset = WavePrefixCountBits(justDied);

        uint deadBase = 0;

        if (deadInWave > 0)
        {
            if (WaveIsFirstLane())
            {
                g_Counters.InterlockedAdd(COUNTER_OFFSET_DEAD, deadInWave, deadBase);
            }

            deadBase = WaveReadLaneFirst(deadBase);

            if (justDied)
            {
                const uint slot = deadBase + deadOffset;

                // Emitが在庫より多く確保しようとした場合、DeadCountが一時的に
                // 実際の在庫を超えることがある。書き込み先だけは必ず範囲内に収める
                if (slot < g_MaxParticles)
                {
                    g_DeadList[slot] = index;
                }
            }
        }
    }
}
