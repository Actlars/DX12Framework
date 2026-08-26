#pragma once

// -------------------------------------------------------------------------------
// ParticleTypes.h
//
// 概要 :
//	GPUパーティクルで CPU と シェーダーが共有する型と定数
//
//	ここの定義とシェーダー側(ParticleCommon.hlsli)の定義は
//	必ず1対1で対応させること
//	どちらか一方だけを直すと、原因の分かりにくい表示崩れになる
//
//	並びの規則 :
//		float4 (16バイト) 単位で切れるように詰める
//		定数バッファは float4 をまたぐメンバを置けないため、
//		パディングを明示して「意図した詰め方」だと分かるようにしている
// -------------------------------------------------------------------------------
namespace Effect
{
	// -------------------------------------------------------------------------------
	// GpuParticle struct
	//
	// 概要 :
	//	パーティクル1粒がGPU上で持つ状態（48バイト）
	//
	//	色や大きさはエミッタ共通のパラメータから毎フレーム求めるため、
	//	粒ごとには持たせない
	//	1粒あたりのバイト数はそのまま帯域に効くので、必要最小限に絞る
	// -------------------------------------------------------------------------------
	struct GpuParticle
	{
		DirectX::XMFLOAT3	Position;		// ワールド座標
		float				Age;			// 生成からの経過時間(秒)

		DirectX::XMFLOAT3	Velocity;		// 速度
		float				LifeTime;		// 寿命(秒)。0以下なら未使用スロット

		float				Seed;			// 粒ごとの乱数種。ばらつきの再現に使う
		float				Rotation;		// ビルボードの回転(ラジアン)
		float				Pad0;
		float				Pad1;
	};

	// -------------------------------------------------------------------------------
	// EmitterShapeGpu : enum
	//	EffectAsset の EmitterShape と同じ並びにしておく
	// -------------------------------------------------------------------------------
	enum class EmitterShapeGpu : uint32_t
	{
		Point	= 0,
		Sphere	= 1,	// エディタ上の Circle に対応（3Dでは球殻）
		Box		= 2,
	};

	// -------------------------------------------------------------------------------
	// EmitterParams struct
	//
	// 概要 :
	//	発生と更新のコンピュートシェーダーが読む定数バッファ（b0）
	//	1エミッタぶんのパラメータをすべてここに載せる
	// -------------------------------------------------------------------------------
	struct EmitterParams
	{
		DirectX::XMFLOAT3	Origin;				// 発生中心（ワールド）
		float				DeltaTime;			// このフレームの経過時間

		DirectX::XMFLOAT3	Gravity;			// 重力加速度
		float				Drag;				// 速度の減衰(1秒あたりの割合)

		float				LifeTime;			// 寿命の基準値
		float				LifeTimeRandom;		// 寿命のばらつき(±)
		float				InitialSpeed;		// 初速の基準値
		float				InitialSpeedRandom;	// 初速のばらつき(±)

		float				SpreadAngle;		// 射出方向の広がり(ラジアン)
		float				ShapeRadius;		// 発生形状の大きさ
		uint32_t			Shape;				// EmitterShapeGpu
		uint32_t			MaxParticles;		// プールの総数

		uint32_t			SpawnCount;			// このフレームに出す数
		uint32_t			RandomSeed;			// フレームごとに変える乱数種
		uint32_t			Pad0;
		uint32_t			Pad1;
	};

	// -------------------------------------------------------------------------------
	// ParticleViewParams struct
	//
	// 概要 :
	//	描画（頂点／ピクセル）が読む定数バッファ（b0）
	//
	//	ビルボードを組み立てるためにカメラの右方向と上方向が要る
	//	ビュー行列から毎回取り出すより、CPUで1回求めて渡すほうが安い
	// -------------------------------------------------------------------------------
	struct ParticleViewParams
	{
		DirectX::XMFLOAT4X4	ViewProjection;

		DirectX::XMFLOAT3	CameraRight;
		float				StartSize;

		DirectX::XMFLOAT3	CameraUp;
		float				EndSize;

		DirectX::XMFLOAT4	StartColor;
		DirectX::XMFLOAT4	EndColor;
	};

	// -------------------------------------------------------------------------------
	// 定数
	// -------------------------------------------------------------------------------

	// コンピュートシェーダーのスレッドグループサイズ
	// シェーダー側の [numthreads] と必ずそろえること
	constexpr uint32_t kParticleThreadGroupSize = 256;

	// カウンタバッファ内のオフセット（バイト）
	// RWByteAddressBuffer へ InterlockedAdd するときの位置
	constexpr uint32_t kCounterOffsetDead	= 0;	// 未使用スロットの残り数
	constexpr uint32_t kCounterOffsetAlive	= 4;	// このフレームに生きている数
	constexpr uint32_t kCounterBufferSize	= 16;	// 16バイト境界に合わせる

	// DrawInstancedIndirect の引数バッファのサイズ（uint4個ぶん）
	constexpr uint32_t kIndirectArgsSize = sizeof(D3D12_DRAW_ARGUMENTS);
}
