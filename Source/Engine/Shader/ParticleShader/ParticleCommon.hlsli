// -------------------------------------------------------------------------------
// ParticleCommon.hlsli
//
// 概要 :
//  GPUパーティクルのシェーダーが共有する「型とユーティリティ」
//
//  CPU側の ParticleTypes.h と 1対1 で対応する
//  片方だけを直すと表示が崩れるため、必ず両方をそろえて変更すること
//
//  定数バッファはここに置かない
//  発生・更新用と描画用で中身が違い、どちらも b0 を使うため、
//  同じ b0 を二重に宣言してしまうことになる
//  発生・更新用は ParticleEmitterParams.hlsli を参照
// -------------------------------------------------------------------------------
#ifndef PARTICLE_COMMON_HLSLI
#define PARTICLE_COMMON_HLSLI

// スレッドグループサイズ。CPU側 kParticleThreadGroupSize と同じ値
#define PARTICLE_THREAD_GROUP_SIZE 256

// カウンタバッファ内のオフセット（バイト）
#define COUNTER_OFFSET_DEAD  0
#define COUNTER_OFFSET_ALIVE 4

// 発生形状
#define EMITTER_SHAPE_POINT  0
#define EMITTER_SHAPE_SPHERE 1
#define EMITTER_SHAPE_BOX    2

// -------------------------------------------------------------------------------
// パーティクル1粒の状態（CPU側 GpuParticle と同じ並び / 48バイト）
// -------------------------------------------------------------------------------
struct GpuParticle
{
    float3  Position;
    float   Age;

    float3  Velocity;
    float   LifeTime;   // 0以下なら未使用スロット

    float   Seed;
    float   Rotation;
    float   Pad0;
    float   Pad1;
};

// -------------------------------------------------------------------------------
// 乱数
//
//  粒ごとに独立したばらつきが欲しいだけなので、質より速度を優先する
//  PCGハッシュは1回の乗算とシフトで十分に散らばり、GPUでも安い
// -------------------------------------------------------------------------------
uint PcgHash(uint _value)
{
    uint state = _value * 747796405u + 2891336453u;
    uint word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// 0.0 ～ 1.0 の乱数
float RandomFloat(inout uint _state)
{
    _state = PcgHash(_state);
    return float(_state & 0x00FFFFFFu) / float(0x01000000u);
}

// -1.0 ～ 1.0 の乱数
float RandomSigned(inout uint _state)
{
    return RandomFloat(_state) * 2.0f - 1.0f;
}

// -------------------------------------------------------------------------------
// 単位球面上のランダムな方向
//
//  極座標で作ると極付近に偏るため、cos(theta) を一様に取って偏りをなくす
// -------------------------------------------------------------------------------
float3 RandomDirection(inout uint _state)
{
    const float z   = RandomSigned(_state);
    const float phi = RandomFloat(_state) * 6.28318530718f;
    const float r   = sqrt(saturate(1.0f - z * z));

    return float3(r * cos(phi), r * sin(phi), z);
}

// -------------------------------------------------------------------------------
// 指定した軸のまわり、広がり角の内側にあるランダムな方向
//
//  SpreadAngle が 0 なら軸そのもの、PI なら全方向になる
// -------------------------------------------------------------------------------
float3 RandomCone(inout uint _state, float3 _axis, float _spreadAngle)
{
    // cos の範囲を制限することで、円錐の内側だけを一様に取る
    const float cosMax = cos(clamp(_spreadAngle, 0.0f, 3.14159265f));
    const float cosT   = lerp(1.0f, cosMax, RandomFloat(_state));
    const float sinT   = sqrt(saturate(1.0f - cosT * cosT));
    const float phi    = RandomFloat(_state) * 6.28318530718f;

    // 軸に垂直な基底を作る。軸とほぼ平行なベクトルを選ばないよう分岐する
    const float3 helper = (abs(_axis.y) < 0.99f) ? float3(0, 1, 0) : float3(1, 0, 0);
    const float3 tangent   = normalize(cross(helper, _axis));
    const float3 bitangent = cross(_axis, tangent);

    return normalize(_axis * cosT + (tangent * cos(phi) + bitangent * sin(phi)) * sinT);
}

#endif // PARTICLE_COMMON_HLSLI
