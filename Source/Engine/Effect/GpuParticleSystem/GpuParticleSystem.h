#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/Effect/ParticleTypes.h>
#include <Engine/RHI/Resource/Buffer/Buffer.h>
#include <Engine/RHI/Resource/Buffer/ConstantBuffer/ConstantBuffer.h>
#include <Engine/RHI/Pipeline/RootSignature/RootSignatureLayout/RootSignatureLayout.h>

namespace RHI { class Device; }

namespace Effect
{
	// -------------------------------------------------------------------------------
	// GpuParticleDesc struct
	//
	// 概要 :
	//	1つのパーティクルシステムを作るときの指定
	// -------------------------------------------------------------------------------
	struct GpuParticleDesc
	{
		// 同時に存在できる粒の最大数
		// プールはこの数で固定確保され、実行中に増減しない
		uint32_t MaxParticles = 8192;
	};

	// -------------------------------------------------------------------------------
	// GpuParticleSystem class
	//
	// 概要 :
	//	発生・更新・描画をすべてGPU上で完結させるパーティクルシステム
	//
	//	CPUが毎フレーム行うのは「パラメータを1つ書き込むこと」だけで、
	//	粒の位置や生死をCPUへ読み戻すことは一度もない
	//	そのためGPU→CPUの同期待ちが発生せず、数万個でも破綻しない
	//
	// 1フレームの流れ :
	//	Update(cmd, params)
	//		1. Emit		 未使用スロットを取り出して新しい粒を初期化
	//		2. Simulate	 全スロットを進め、生存リストと未使用リストを作り直す
	//		3. Finalize	 生存数から間接描画の引数を組み立て、カウンタを戻す
	//	Render(cmd, view)
	//		4. DrawInstancedIndirect でビルボードをまとめて描く
	//
	// 責務の分担 :
	//	GpuParticleSystem	GPUリソースとディスパッチの管理（このクラス）
	//	EmitterParams		どんなエフェクトか（呼び出し側が毎フレーム渡す）
	//	各パネル / シーン	いつ・どこへ描くか
	//
	// 最適化の要点 :
	//	発生と生存判定では、スレッドごとに原子加算を行うと
	//	同一アドレスへのアクセスが直列化して律速する
	//	波(wave)単位で件数をまとめ、代表レーンが1回だけ加算することで
	//	原子加算の回数を波幅ぶんの1（32～64分の1）に減らしている
	//	詳細はシェーダー側のコメントを参照
	// -------------------------------------------------------------------------------
	class GpuParticleSystem
	{
	public:

		GpuParticleSystem() = default;
		~GpuParticleSystem();

		// -------------------------------------------------------------------------------
		// @brief	GPUリソースとパイプラインを用意する
		//
		// @param[in]	_pDevice	デバイス
		// @param[in]	_desc		最大数などの指定
		// -------------------------------------------------------------------------------
		bool Init(RHI::Device* _pDevice, const GpuParticleDesc& _desc);

		void Term();

		// -------------------------------------------------------------------------------
		// @brief	発生・更新・間接描画引数の作成を1フレームぶん実行する
		//
		//	描画先の設定には依存しないため、レンダーターゲットを切り替える前に呼んでよい
		//
		// @param[in]	_pCmd	記録中のコマンドリスト
		// @param[in]	_params	このフレームのエミッタ設定
		// -------------------------------------------------------------------------------
		void Update(ID3D12GraphicsCommandList* _pCmd, const EmitterParams& _params);

		// -------------------------------------------------------------------------------
		// @brief	生存している粒をビルボードとして描く
		//
		//	呼び出し前にレンダーターゲットとビューポートを設定しておくこと
		//	描画数はGPU側が決めるため、CPUは件数を知らないまま発行できる
		//
		// @param[in]	_pCmd	記録中のコマンドリスト
		// @param[in]	_view	カメラ行列と色・大きさの指定
		// -------------------------------------------------------------------------------
		void Render(ID3D12GraphicsCommandList* _pCmd, const ParticleViewParams& _view);

		// -------------------------------------------------------------------------------
		// @brief	すべての粒を消し、未使用リストを初期状態へ戻す
		//
		//	次の Update の中で反映される
		// -------------------------------------------------------------------------------
		void RequestReset();

		bool		IsValid()			const { return m_Initialized; }
		uint32_t	GetMaxParticles()	const { return m_MaxParticles; }

	private:

		// バッファ一式を確保し、未使用リストの初期値を書き込む
		bool CreateBuffers();

		// ルートシグネチャとPSOを用意する
		bool CreatePipelines();

		// UAVへの書き込み完了を、次のディスパッチが読む前に保証する
		void InsertUavBarrier(ID3D12GraphicsCommandList* _pCmd);

		// 未使用リストとカウンタを初期状態へ書き戻す
		void UploadInitialState(ID3D12GraphicsCommandList* _pCmd);

		// 共通の定数バッファとUAVをまとめてバインドする
		void BindComputeResources(ID3D12GraphicsCommandList* _pCmd);

		RHI::Device* m_pDevice = nullptr;	// 所有権なし

		// -------------------------------------------------------------------------------
		// GPUリソース
		// -------------------------------------------------------------------------------
		RHI::Buffer m_ParticleBuffer;	// 粒の本体（プール）
		RHI::Buffer m_DeadListBuffer;	// 未使用スロットの索引
		RHI::Buffer m_AliveListBuffer;	// このフレームに描く索引
		RHI::Buffer m_CounterBuffer;	// 未使用数 / 生存数
		RHI::Buffer m_IndirectArgs;		// DrawInstancedIndirect の引数
		RHI::Buffer m_InitialStateUpload;	// 初期化用の中身（Uploadヒープ）

		RHI::ConstantBuffer m_EmitterCB;
		RHI::ConstantBuffer m_ViewCB;

		// -------------------------------------------------------------------------------
		// パイプライン
		// -------------------------------------------------------------------------------
		RHI::RootSignatureLayout m_ComputeRootSignature;
		RHI::RootSignatureLayout m_RenderRootSignature;

		ID3D12PipelineState* m_pEmitPSO		= nullptr;	// PipelineCacheが所有
		ID3D12PipelineState* m_pSimulatePSO	= nullptr;
		ID3D12PipelineState* m_pFinalizePSO	= nullptr;
		ID3D12PipelineState* m_pRenderPSO	= nullptr;

		ComPtr<ID3D12CommandSignature> m_pDrawCommandSignature;

		uint32_t m_MaxParticles	= 0;
		uint32_t m_FrameCounter	= 0;	// 乱数種をフレームごとに変えるために使う

		bool m_Initialized	= false;
		bool m_NeedsReset	= true;		// 次のUpdateで初期状態を書き込む

		GpuParticleSystem	(const GpuParticleSystem&) = delete;
		void operator =		(const GpuParticleSystem&) = delete;
	};
}
