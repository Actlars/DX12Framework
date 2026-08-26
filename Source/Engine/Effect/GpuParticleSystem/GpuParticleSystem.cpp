// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "GpuParticleSystem.h"

#include <Engine/RHI/Core/Device/Device.h>
#include <Engine/RHI/Resource/ResourceStateTracker/ResourceStateTracker.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

namespace
{
	// 設定ファイルの場所
	constexpr const wchar_t* kComputeRootSignaturePath	= L"Assets/Config/Json/RootSignature/GpuParticleCompute.json";
	constexpr const wchar_t* kRenderRootSignaturePath	= L"Assets/Config/Json/RootSignature/GpuParticleRender.json";

	constexpr const wchar_t* kEmitPsoPath		= L"Assets/Config/Json/PipelineState/ParticleEmit.json";
	constexpr const wchar_t* kSimulatePsoPath	= L"Assets/Config/Json/PipelineState/ParticleSimulate.json";
	constexpr const wchar_t* kFinalizePsoPath	= L"Assets/Config/Json/PipelineState/ParticleFinalize.json";
	constexpr const wchar_t* kRenderPsoPath		= L"Assets/Config/Json/PipelineState/ParticleRender.json";

	// スレッド数からディスパッチするグループ数を求める
	uint32_t DivideRoundUp(uint32_t _value, uint32_t _divisor)
	{
		return (_value + _divisor - 1) / _divisor;
	}
}

Effect::GpuParticleSystem::~GpuParticleSystem()
{
	Term();
}

// -------------------------------------------------------------------------------
// 初期化
// -------------------------------------------------------------------------------
bool Effect::GpuParticleSystem::Init(RHI::Device* _pDevice, const GpuParticleDesc& _desc)
{
	if (_pDevice == nullptr)
	{
		ELOG("GpuParticleSystem::Init() device is nullptr");
		return false;
	}

	m_pDevice = _pDevice;

	// スレッドグループの倍数に切り上げる
	// 端数があるとシェーダー側で毎回範囲確認が必要になり、無駄が増える
	m_MaxParticles = DivideRoundUp((std::max)(_desc.MaxParticles, 1u), kParticleThreadGroupSize)
		* kParticleThreadGroupSize;

	if (!CreateBuffers())
	{
		Term();
		return false;
	}

	if (!CreatePipelines())
	{
		Term();
		return false;
	}

	m_Initialized	= true;
	m_NeedsReset	= true;
	return true;
}

void Effect::GpuParticleSystem::Term()
{
	if (m_pDevice != nullptr)
	{
		// -------------------------------------------------------------------------------
		// 解放の前に、GPUがこれらのバッファを使い終わるのを待つ
		//
		// コマンドリストはCPUより1～2フレーム遅れて実行される
		// 待たずに解放すると、実行中のコマンドが消えたリソースを参照し、
		// デバイスロスト（画面が固まり、終了時に大量のリーク報告が出る）になる
		// -------------------------------------------------------------------------------
		m_pDevice->WaitForGPU();

		// 追跡表に古いポインタを残すと、同じアドレスに別リソースが載ったとき誤動作する
		auto* pTracker = m_pDevice->GetResourceStateTracker();

		pTracker->UnRegisterResource(m_ParticleBuffer.GetResource());
		pTracker->UnRegisterResource(m_DeadListBuffer.GetResource());
		pTracker->UnRegisterResource(m_AliveListBuffer.GetResource());
		pTracker->UnRegisterResource(m_CounterBuffer.GetResource());
		pTracker->UnRegisterResource(m_IndirectArgs.GetResource());
	}

	m_pDrawCommandSignature.Reset();

	m_InitialStateUpload.Term();
	m_IndirectArgs.Term();
	m_CounterBuffer.Term();
	m_AliveListBuffer.Term();
	m_DeadListBuffer.Term();
	m_ParticleBuffer.Term();

	m_ViewCB.Term();
	m_EmitterCB.Term();

	// PSOはPipelineCacheが所有しているため、ここでは参照を捨てるだけ
	m_pEmitPSO		= nullptr;
	m_pSimulatePSO	= nullptr;
	m_pFinalizePSO	= nullptr;
	m_pRenderPSO	= nullptr;

	m_pDevice		= nullptr;
	m_Initialized	= false;
}

// -------------------------------------------------------------------------------
// バッファの確保
// -------------------------------------------------------------------------------
bool Effect::GpuParticleSystem::CreateBuffers()
{
	auto* pD3DDevice = m_pDevice->GetDevice();

	// -------------------------------------------------------------------------------
	// GPUだけが読み書きするバッファはすべてDefaultヒープ + UAV許可で確保する
	// ルートディスクリプタとしてバインドするため、ビューの事前生成は要らない
	// -------------------------------------------------------------------------------
	const auto createDefaultUav = [&](RHI::Buffer& _buffer, size_t _size, const char* _name)
	{
		RHI::BufferDesc desc;
		desc.SizeInBytes	= _size;
		desc.HeapType		= RHI::BufferHeapType::Default;
		desc.AllowUAV		= true;

		if (!_buffer.Init(pD3DDevice, desc))
		{
			ELOG("GpuParticleSystem::CreateBuffers() failed : %s", _name);
			return false;
		}

		// -------------------------------------------------------------------------------
		// 生成直後の状態を追跡表へ登録しておく
		//
		// バッファは必ず COMMON で作られるため、その通りに登録する
		// 実際と違う状態を登録すると、最初のバリアで誤った遷移元が使われてしまう
		// -------------------------------------------------------------------------------
		m_pDevice->GetResourceStateTracker()->RegisterResource(
			_buffer.GetResource(), D3D12_RESOURCE_STATE_COMMON);

		return true;
	};

	if (!createDefaultUav(m_ParticleBuffer, sizeof(GpuParticle) * m_MaxParticles, "ParticleBuffer"))	{ return false; }
	if (!createDefaultUav(m_DeadListBuffer, sizeof(uint32_t) * m_MaxParticles, "DeadList"))			{ return false; }
	if (!createDefaultUav(m_AliveListBuffer, sizeof(uint32_t) * m_MaxParticles, "AliveList"))		{ return false; }
	if (!createDefaultUav(m_CounterBuffer, kCounterBufferSize, "CounterBuffer"))						{ return false; }
	if (!createDefaultUav(m_IndirectArgs, kIndirectArgsSize, "IndirectArgs"))						{ return false; }

	// -------------------------------------------------------------------------------
	// 初期状態を書き込むためのUploadバッファ
	//
	//	未使用リスト（0 ～ MaxParticles-1）とカウンタをまとめて1本に詰め、
	//	リセットのたびにCopyBufferRegionで転送する
	//	毎回作り直さず持ち続けることで、リセットが確保を伴わない安い操作になる
	// -------------------------------------------------------------------------------
	//	[0]                        未使用リスト
	//	[deadListBytes]            カウンタ
	//	[deadListBytes + counter]  粒プールを埋める0
	//
	//	Defaultヒープの中身は生成直後は不定なので、
	//	寿命に大きな値が入っていると「使用中」と誤認されてしまう
	//	リセット時に0で埋めることで、確実に未使用の状態から始められる
	const size_t deadListBytes	= sizeof(uint32_t) * m_MaxParticles;
	const size_t particleBytes	= sizeof(GpuParticle) * m_MaxParticles;
	const size_t uploadBytes	= deadListBytes + kCounterBufferSize + particleBytes;

	RHI::BufferDesc uploadDesc;
	uploadDesc.SizeInBytes	= uploadBytes;
	uploadDesc.HeapType		= RHI::BufferHeapType::Upload;

	if (!m_InitialStateUpload.Init(pD3DDevice, uploadDesc))
	{
		ELOG("GpuParticleSystem::CreateBuffers() failed : InitialStateUpload");
		return false;
	}

	{
		// 0で埋めておき、必要な場所だけ上書きする
		std::vector<uint32_t> initialData(uploadBytes / sizeof(uint32_t), 0);

		// 未使用リストの中身。末尾から取り出すので順序はどちらでもよい
		for (uint32_t i = 0; i < m_MaxParticles; ++i)
		{
			initialData[i] = i;
		}

		// カウンタ部 : 未使用数 = 全部、生存数 = 0
		initialData[m_MaxParticles + 0] = m_MaxParticles;
		initialData[m_MaxParticles + 1] = 0;

		// 粒プールぶんは0のまま（寿命0 = 未使用）

		m_InitialStateUpload.Write(initialData.data(), uploadBytes);
	}

	// -------------------------------------------------------------------------------
	// 定数バッファ
	// -------------------------------------------------------------------------------
	auto* pResPool = m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES);

	if (!m_EmitterCB.Init(pD3DDevice, pResPool, sizeof(EmitterParams)) ||
		!m_ViewCB.Init(pD3DDevice, pResPool, sizeof(ParticleViewParams)))
	{
		ELOG("GpuParticleSystem::CreateBuffers() failed : ConstantBuffer");
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------
// パイプラインの用意
// -------------------------------------------------------------------------------
bool Effect::GpuParticleSystem::CreatePipelines()
{
	auto* pD3DDevice	= m_pDevice->GetDevice();
	auto* pCache		= m_pDevice->GetPipelineCache();

	if (!m_ComputeRootSignature.LoadFromJson(pD3DDevice, kComputeRootSignaturePath) ||
		!m_RenderRootSignature.LoadFromJson(pD3DDevice, kRenderRootSignaturePath))
	{
		ELOG("GpuParticleSystem::CreatePipelines() RootSignature load failed");
		return false;
	}

	// 頂点バッファを使わないため、入力レイアウトは空でよい
	const D3D12_INPUT_LAYOUT_DESC emptyLayout{ nullptr, 0 };

	m_pEmitPSO		= pCache->GetOrCreate(pD3DDevice, kEmitPsoPath,		m_ComputeRootSignature.GetRootSignature(), emptyLayout);
	m_pSimulatePSO	= pCache->GetOrCreate(pD3DDevice, kSimulatePsoPath,	m_ComputeRootSignature.GetRootSignature(), emptyLayout);
	m_pFinalizePSO	= pCache->GetOrCreate(pD3DDevice, kFinalizePsoPath,	m_ComputeRootSignature.GetRootSignature(), emptyLayout);
	m_pRenderPSO	= pCache->GetOrCreate(pD3DDevice, kRenderPsoPath,	m_RenderRootSignature.GetRootSignature(), emptyLayout);

	if (m_pEmitPSO == nullptr || m_pSimulatePSO == nullptr ||
		m_pFinalizePSO == nullptr || m_pRenderPSO == nullptr)
	{
		ELOG("GpuParticleSystem::CreatePipelines() PSO creation failed");
		return false;
	}

	// -------------------------------------------------------------------------------
	// 間接描画のコマンドシグネチャ
	//
	//	引数バッファの中身をどう解釈するかをGPUへ教えるもの
	//	ルート引数を差し替えない単純な描画なので、ByteStrideは引数1組ぶんでよい
	// -------------------------------------------------------------------------------
	D3D12_INDIRECT_ARGUMENT_DESC argumentDesc{};
	argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

	D3D12_COMMAND_SIGNATURE_DESC signatureDesc{};
	signatureDesc.ByteStride		= kIndirectArgsSize;
	signatureDesc.NumArgumentDescs	= 1;
	signatureDesc.pArgumentDescs	= &argumentDesc;

	if (FAILED(pD3DDevice->CreateCommandSignature(
		&signatureDesc, nullptr, IID_PPV_ARGS(m_pDrawCommandSignature.GetAddressOf()))))
	{
		ELOG("GpuParticleSystem::CreatePipelines() CreateCommandSignature failed");
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------
// 1フレームぶんの更新
// -------------------------------------------------------------------------------
void Effect::GpuParticleSystem::Update(ID3D12GraphicsCommandList* _pCmd, const EmitterParams& _params)
{
	if (!m_Initialized || _pCmd == nullptr)
	{ return; }

	// -------------------------------------------------------------------------------
	// バッファを書き込み可能な状態にそろえる
	//
	// 前フレームに描画まで進んだかどうかで状態が変わるため、
	// ここで必ず UNORDERED_ACCESS へ戻してから始める
	// （描画されないフレームがあると、状態が食い違ったまま次の更新へ入ってしまう）
	// -------------------------------------------------------------------------------
	{
		auto* pTracker = m_pDevice->GetResourceStateTracker();

		pTracker->TransitionResource(m_ParticleBuffer.GetResource(),  D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		pTracker->TransitionResource(m_AliveListBuffer.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		pTracker->TransitionResource(m_IndirectArgs.GetResource(),    D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		pTracker->FlushBarriers(_pCmd);
	}

	// -------------------------------------------------------------------------------
	// リセット要求があれば、未使用リストとカウンタを初期状態へ戻す
	// -------------------------------------------------------------------------------
	if (m_NeedsReset)
	{
		UploadInitialState(_pCmd);
		m_NeedsReset = false;
	}

	// -------------------------------------------------------------------------------
	// このフレームのパラメータを書き込む
	//
	// 乱数種はフレームごとに変える
	// 固定にすると、毎フレーム同じ位置・同じ速度の粒しか出なくなる
	// -------------------------------------------------------------------------------
	EmitterParams params = _params;
	params.MaxParticles	= m_MaxParticles;
	params.RandomSeed	= ++m_FrameCounter * 2654435761u;

	// 1フレームに出せる数はプールの上限を超えられない
	params.SpawnCount = (std::min)(params.SpawnCount, m_MaxParticles);

	if (auto* pDest = m_EmitterCB.GetPtr<EmitterParams>())
	{
		*pDest = params;
	}

	_pCmd->SetComputeRootSignature(m_ComputeRootSignature.GetRootSignature());
	BindComputeResources(_pCmd);

	// -------------------------------------------------------------------------------
	// 1. 発生
	//    出す数が0のフレームはディスパッチ自体を省く
	// -------------------------------------------------------------------------------
	if (params.SpawnCount > 0)
	{
		_pCmd->SetPipelineState(m_pEmitPSO);
		_pCmd->Dispatch(DivideRoundUp(params.SpawnCount, kParticleThreadGroupSize), 1, 1);

		// Emitの書き込みをSimulateが読むため、完了を待たせる
		InsertUavBarrier(_pCmd);
	}

	// -------------------------------------------------------------------------------
	// 2. 更新（生存リストと未使用リストの作り直し）
	// -------------------------------------------------------------------------------
	_pCmd->SetPipelineState(m_pSimulatePSO);
	_pCmd->Dispatch(DivideRoundUp(m_MaxParticles, kParticleThreadGroupSize), 1, 1);

	InsertUavBarrier(_pCmd);

	// -------------------------------------------------------------------------------
	// 3. 間接描画の引数を作る
	// -------------------------------------------------------------------------------
	_pCmd->SetPipelineState(m_pFinalizePSO);
	_pCmd->Dispatch(1, 1, 1);

	// 引数バッファは描画時に INDIRECT_ARGUMENT として読まれる
	auto* pTracker = m_pDevice->GetResourceStateTracker();
	pTracker->TransitionResource(m_IndirectArgs.GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
	pTracker->FlushBarriers(_pCmd);
}

// -------------------------------------------------------------------------------
// 描画
// -------------------------------------------------------------------------------
void Effect::GpuParticleSystem::Render(ID3D12GraphicsCommandList* _pCmd, const ParticleViewParams& _view)
{
	if (!m_Initialized || _pCmd == nullptr)
	{ return; }

	if (auto* pDest = m_ViewCB.GetPtr<ParticleViewParams>())
	{
		*pDest = _view;
	}

	// -------------------------------------------------------------------------------
	// 粒のバッファは、直前まで書き込み先(UAV)だったものを読み取りへ切り替える
	// -------------------------------------------------------------------------------
	auto* pTracker = m_pDevice->GetResourceStateTracker();
	pTracker->TransitionResource(m_ParticleBuffer.GetResource(),  D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	pTracker->TransitionResource(m_AliveListBuffer.GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	pTracker->FlushBarriers(_pCmd);

	_pCmd->SetGraphicsRootSignature(m_RenderRootSignature.GetRootSignature());
	_pCmd->SetPipelineState(m_pRenderPSO);

	_pCmd->SetGraphicsRootConstantBufferView(
		m_RenderRootSignature.GetSlot("ViewParams"), m_ViewCB.GetAddress());
	_pCmd->SetGraphicsRootShaderResourceView(
		m_RenderRootSignature.GetSlot("Particles"), m_ParticleBuffer.GetAddress());
	_pCmd->SetGraphicsRootShaderResourceView(
		m_RenderRootSignature.GetSlot("AliveList"), m_AliveListBuffer.GetAddress());

	_pCmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// -------------------------------------------------------------------------------
	// 描画数はGPUが決める
	// CPUは件数を知らないまま1回の命令で発行できるため、読み戻しの待ちが起きない
	// -------------------------------------------------------------------------------
	_pCmd->ExecuteIndirect(
		m_pDrawCommandSignature.Get(), 1, m_IndirectArgs.GetResource(), 0, nullptr, 0);

	// 次のフレームの更新に備えて、書き込み可能な状態へ戻す
	pTracker->TransitionResource(m_ParticleBuffer.GetResource(),  D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	pTracker->TransitionResource(m_AliveListBuffer.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	pTracker->TransitionResource(m_IndirectArgs.GetResource(),    D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	pTracker->FlushBarriers(_pCmd);
}

void Effect::GpuParticleSystem::RequestReset()
{
	m_NeedsReset = true;
}

// -------------------------------------------------------------------------------
// 共通リソースのバインド
// -------------------------------------------------------------------------------
void Effect::GpuParticleSystem::BindComputeResources(ID3D12GraphicsCommandList* _pCmd)
{
	const auto& rs = m_ComputeRootSignature;

	_pCmd->SetComputeRootConstantBufferView(rs.GetSlot("EmitterParams"), m_EmitterCB.GetAddress());
	_pCmd->SetComputeRootUnorderedAccessView(rs.GetSlot("Particles"),    m_ParticleBuffer.GetAddress());
	_pCmd->SetComputeRootUnorderedAccessView(rs.GetSlot("DeadList"),     m_DeadListBuffer.GetAddress());
	_pCmd->SetComputeRootUnorderedAccessView(rs.GetSlot("AliveList"),    m_AliveListBuffer.GetAddress());
	_pCmd->SetComputeRootUnorderedAccessView(rs.GetSlot("Counters"),     m_CounterBuffer.GetAddress());
	_pCmd->SetComputeRootUnorderedAccessView(rs.GetSlot("IndirectArgs"), m_IndirectArgs.GetAddress());
}

// -------------------------------------------------------------------------------
// UAVバリア
//
// 同じバッファに対する「前のディスパッチの書き込み」と
// 「次のディスパッチの読み取り」の順序を保証する
// 状態は変わらないため、遷移バリアではなくUAVバリアを使う
// -------------------------------------------------------------------------------
void Effect::GpuParticleSystem::InsertUavBarrier(ID3D12GraphicsCommandList* _pCmd)
{
	D3D12_RESOURCE_BARRIER barriers[4]{};

	const auto makeUav = [](ID3D12Resource* _pResource)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type			= D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.UAV.pResource	= _pResource;
		return barrier;
	};

	barriers[0] = makeUav(m_ParticleBuffer.GetResource());
	barriers[1] = makeUav(m_DeadListBuffer.GetResource());
	barriers[2] = makeUav(m_AliveListBuffer.GetResource());
	barriers[3] = makeUav(m_CounterBuffer.GetResource());

	_pCmd->ResourceBarrier(_countof(barriers), barriers);
}

// -------------------------------------------------------------------------------
// 初期状態の書き込み
//
// 未使用リストとカウンタを、あらかじめ用意したUploadバッファから転送する
// コンピュートシェーダーを1本増やさずに済むうえ、確実に初期化できる
// -------------------------------------------------------------------------------
void Effect::GpuParticleSystem::UploadInitialState(ID3D12GraphicsCommandList* _pCmd)
{
	auto* pTracker = m_pDevice->GetResourceStateTracker();

	// コピー先として受け取れる状態にする
	pTracker->TransitionResource(m_DeadListBuffer.GetResource(), D3D12_RESOURCE_STATE_COPY_DEST);
	pTracker->TransitionResource(m_CounterBuffer.GetResource(),  D3D12_RESOURCE_STATE_COPY_DEST);
	pTracker->TransitionResource(m_ParticleBuffer.GetResource(), D3D12_RESOURCE_STATE_COPY_DEST);
	pTracker->FlushBarriers(_pCmd);

	const UINT64 deadListBytes = sizeof(uint32_t)    * static_cast<UINT64>(m_MaxParticles);
	const UINT64 particleBytes = sizeof(GpuParticle) * static_cast<UINT64>(m_MaxParticles);

	_pCmd->CopyBufferRegion(
		m_DeadListBuffer.GetResource(), 0,
		m_InitialStateUpload.GetResource(), 0,
		deadListBytes);

	_pCmd->CopyBufferRegion(
		m_CounterBuffer.GetResource(), 0,
		m_InitialStateUpload.GetResource(), deadListBytes,
		kCounterBufferSize);

	// 粒プールを0で埋める。寿命0が「未使用」の目印になる
	_pCmd->CopyBufferRegion(
		m_ParticleBuffer.GetResource(), 0,
		m_InitialStateUpload.GetResource(), deadListBytes + kCounterBufferSize,
		particleBytes);

	pTracker->TransitionResource(m_DeadListBuffer.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	pTracker->TransitionResource(m_CounterBuffer.GetResource(),  D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	pTracker->TransitionResource(m_ParticleBuffer.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	pTracker->FlushBarriers(_pCmd);
}
