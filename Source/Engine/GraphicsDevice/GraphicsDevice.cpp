// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "GraphicsDevice.h"

#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
GraphicsDevice::GraphicsDevice()
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
GraphicsDevice::~GraphicsDevice() 
{ Term(); }


// -------------------------------------------------------------------------------
//		初期化
// -------------------------------------------------------------------------------
bool GraphicsDevice::Init(const Desc& _desc)
{
	m_Desc = _desc;

	// 各サブシステムを順番に初期化
	// 失敗した時点で終了処理を呼んで安全に抜ける
	if (!InitDevice())			{ Term(); return false; }
	if (!InitCommandQueue())	{ Term(); return false; }
	if (!InitSwapChain())		{ Term(); return false; }
	if (!InitDescriptorPools()) { Term(); return false; }
	if (!InitColorTargets())	{ Term(); return false; }
	if (!InitDepthTarget())		{ Term(); return false; }
	if (!InitCommandList())		{ Term(); return false; }
	if (!InitFence())			{ Term(); return false; }

	// ビューポートとシザー矩形の設定
	m_Viewport.TopLeftX	= 0.0f;
	m_Viewport.TopLeftY = 0.0f;
	m_Viewport.Width	= static_cast<float>(_desc.Width);
	m_Viewport.Height	= static_cast<float>(_desc.Height);
	m_Viewport.MinDepth = 0.0f;
	m_Viewport.MaxDepth = 1.0f;

	m_Scissor.left		= 0;
	m_Scissor.top		= 0;
	m_Scissor.right		= static_cast<LONG>(_desc.Width);
	m_Scissor.bottom	= static_cast<LONG>(_desc.Height);

	return true;
}

// -------------------------------------------------------------------------------
//		終了処理
// -------------------------------------------------------------------------------
void GraphicsDevice::Term()
{
	// GPUの全処理が終わるまで待つ
	// 描画中のリソースを解放するとGPUが壊れたメモリにアクセスするため待機
	m_Fence.Sync(m_pQueue.Get());

	// 各リソースを逆順に解放
	m_Fence.Term();
	m_CommandList.Term();
	m_DepthTarget.Term();

	//for (auto& ct : m_ColorTargets) 
	//{ ct->Term(); }
	m_ColorTargets.clear();

	// DescriptorPool は参照カウント管理なのでRelease()を呼ぶ
	for (auto i = 0u; i < POOL_COUNT; ++i)
	{
		if (m_pPool[i] != nullptr)
		{
			m_pPool[i]->Release();
			m_pPool[i] = nullptr;
		}
	}

	m_pSwapChain.Reset();
	m_pQueue.Reset();
	m_pDevice.Reset();
}

// -------------------------------------------------------------------------------
//		GPU完了待ち
// -------------------------------------------------------------------------------
void GraphicsDevice::WaitForGPU()
{
	// m_pQueueがnullptrなら何もしない
	if (m_pQueue == nullptr) 
	{ return; }

	m_Fence.Sync(m_pQueue.Get());
}

// -------------------------------------------------------------------------------
//		フレーム開始処理
// -------------------------------------------------------------------------------
ID3D12GraphicsCommandList* GraphicsDevice::BeginFrame()
{
	// コマンドリストのリセット
	// 前フレームの命令が残ったままだと記録できないのでリセットする
	m_pCurrentCmd = m_CommandList.Reset();

	// バックバッファを描画先に切り替えるリソースバリア
	// PRESENT状態のままRTVとして使おうとするとGPUエラーになる
	auto* pTarget = m_ColorTargets[m_FrameIndex]->GetResource();
	D3D12_RESOURCE_BARRIER barrier	= {};
	barrier.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags					= D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource	= pTarget;
	barrier.Transition.StateBefore	= D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter	= D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_pCurrentCmd->ResourceBarrier(1, &barrier);

	// レンダーターゲットと深度バッファをクリア
	auto handleRTV = m_ColorTargets[m_FrameIndex]->GetHandleRTV()->HandleCPU;
	auto handleDSV = m_DepthTarget.GetHandleDSV()->HandleCPU;

	m_pCurrentCmd->ClearRenderTargetView(handleRTV, CLEAR_COLOR, 0, nullptr);
	m_pCurrentCmd->ClearDepthStencilView(handleDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// レンダーターゲットを設定
	m_pCurrentCmd->OMSetRenderTargets(1, &handleRTV, FALSE, &handleDSV);

	// ビューポート / シザー矩形を設定
	m_pCurrentCmd->RSSetViewports(1, &m_Viewport);
	m_pCurrentCmd->RSSetScissorRects(1, &m_Scissor);

	return m_pCurrentCmd;
}

// -------------------------------------------------------------------------------
//		フレーム終了処理
// -------------------------------------------------------------------------------
void GraphicsDevice::EndFrame()
{
	//auto* pCmd		= m_CommandList.Reset();
	auto* pTarget	= m_ColorTargets[m_FrameIndex]->GetResource();

	// バックバッファを表示用に切り替えるバリア
	// RENDER_TARGETのままPresentしようとするとGPUエラーになる
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = pTarget;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_pCurrentCmd->ResourceBarrier(1, &barrier);

	// コマンドリストをクローズしてGPUに投入
	m_pCurrentCmd->Close();

	ID3D12CommandList* ppLists[] = { m_pCurrentCmd };
	m_pQueue->ExecuteCommandLists(1, ppLists);
}

// -------------------------------------------------------------------------------
//		画面表示
// -------------------------------------------------------------------------------
void GraphicsDevice::Present(uint32_t _syncInterval)
{
	// バックバッファを画面に表示
	m_pSwapChain->Present(_syncInterval, 0);

	// GPU完了を待つ
	// 次フレームで同じバッファに書き込む前にGPUが使い終わるのを確認
	m_Fence.Wait(m_pQueue.Get(), INFINITE);

	// フレームインデックスを更新
	// スワップチェインが管理するバックバッファ番号に合わせる
	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
}

// -------------------------------------------------------------------------------
//		ゲッター
// -------------------------------------------------------------------------------
ID3D12Device*		GraphicsDevice::GetDevice()			const { return m_pDevice.Get(); }
ID3D12CommandQueue* GraphicsDevice::GetQueue()			const { return m_pQueue.Get(); }
IDXGISwapChain3*	GraphicsDevice::GetSwapChain()		const { return m_pSwapChain.Get(); }
CommandList*		GraphicsDevice::GetCommandList()		  { return &m_CommandList; }
DescriptorPool*		GraphicsDevice::GetPool(POOL_TYPE _type) const { return m_pPool[_type]; }
DepthTarget*		GraphicsDevice::GetDepthTarget()	const { return const_cast<DepthTarget*>(&m_DepthTarget); }
uint32_t            GraphicsDevice::GetFrameIndex()		const { return m_FrameIndex; }
uint32_t            GraphicsDevice::GetFrameCount()		const { return m_Desc.FrameCount; }
uint32_t            GraphicsDevice::GetWidth()			const { return m_Desc.Width; }
uint32_t            GraphicsDevice::GetHeight()			const { return m_Desc.Height; }

ColorTarget* GraphicsDevice::GetColorTarget(uint32_t _index) const
{
	if (_index >= m_ColorTargets.size()) 
	{ return nullptr; }
	//return const_cast<ColorTarget*>(&m_ColorTargets[_index]);
	return m_ColorTargets[_index].get();
}

// -------------------------------------------------------------------------------
//		private
// -------------------------------------------------------------------------------

// -------------------------------------------------------------------------------
//		デバイス生成
// -------------------------------------------------------------------------------
bool GraphicsDevice::InitDevice()
{
#if defined(DEBUG) || defined(_DEBUG)
	// デバッグビルド時はデバッグレイヤーを有効化
	// デバッグレイヤーはDX12の使用法エラーをログに出力する
	{
		ComPtr<ID3D12Debug> pDebug;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(pDebug.GetAddressOf()))))
		{ pDebug->EnableDebugLayer(); }
	}
#endif

	// DXGIFactory6 を生成（EnumAdapterByGpuPreference のために必要）
	ComPtr<IDXGIFactory6> pFactory;
	if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(pFactory.GetAddressOf()))))
	{
		ELOG("Error : CreateDXGIFactory2() Failed");
		return false;
	}

	// ハイパフォーマンスGPU優先でアダプタを列挙し、最初に作れたものを採用
	ComPtr<IDXGIAdapter1> pAdapter;
	for (UINT i = 0;
		pFactory->EnumAdapterByGpuPreference(
			i,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(pAdapter.ReleaseAndGetAddressOf())) != DXGI_ERROR_NOT_FOUND;
		++i)
	{
		DXGI_ADAPTER_DESC1 adapterDesc = {};
		pAdapter->GetDesc1(&adapterDesc);

		// ソフトウェアアダプタ(WARP)は除外
		if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		// levels を上から試す
		for (auto lv : levels)
		{
			if (SUCCEEDED(D3D12CreateDevice(
				pAdapter.Get(),
				lv,
				IID_PPV_ARGS(m_pDevice.ReleaseAndGetAddressOf()))))
			{
				ELOG("Selected GPU : %ls", adapterDesc.Description);
				return true;   // 採用して即抜ける
			}
		}
	}

	ELOG("Error : No suitable D3D12 device found.");
	return false;
}

// -------------------------------------------------------------------------------
//		コマンドキュー生成
// -------------------------------------------------------------------------------
bool GraphicsDevice::InitCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type		= D3D12_COMMAND_LIST_TYPE_DIRECT;		// Graphics / Compute/ Copy すべて実行可能
	desc.Priority	= D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags		= D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask	= 0;

	auto hr = m_pDevice->CreateCommandQueue(
		&desc,
		IID_PPV_ARGS(m_pQueue.GetAddressOf()));
	if (FAILED(hr))
	{ return false; }

	return true;
}
// -------------------------------------------------------------------------------
//		スワップチェイン生成
// -------------------------------------------------------------------------------
bool GraphicsDevice::InitSwapChain()
{
	// DXGIFactory を生成
	// スワップチェインはDXGI（DirectX Graphics Infrastructure）が管理する
	ComPtr<IDXGIFactory4> pFactory;
	auto hr = CreateDXGIFactory2(0, IID_PPV_ARGS(pFactory.GetAddressOf()));
	if(FAILED(hr))
	{
		ELOG("Error : CreteDXGIFactory2() Failed");
		return false;
	}

	// スワップチェインの設定
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.Width				= m_Desc.Width;
	desc.Height				= m_Desc.Height;
	desc.Format				= m_Desc.Format;
	desc.Stereo				= FALSE;
	desc.SampleDesc.Count	= 1;
	desc.SampleDesc.Quality = 0;
	desc.BufferUsage		= DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount		= m_Desc.FrameCount;				// バックバッファ数
	desc.Scaling			= DXGI_SCALING_STRETCH;				// バックバッファーは伸び縮み可能
	desc.SwapEffect			= DXGI_SWAP_EFFECT_FLIP_DISCARD;	// フリップ後は速やかに破棄
	desc.AlphaMode			= DXGI_ALPHA_MODE_UNSPECIFIED;
	desc.Flags				= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;	// ウィンドウ⇔フルスクリーン切り替え可能

	ComPtr<IDXGISwapChain1> pSwapChain1;
	hr = pFactory->CreateSwapChainForHwnd(
		m_pQueue.Get(),		// コマンドキューを渡す
		m_Desc.hWnd,
		&desc,
		nullptr,
		nullptr,
		pSwapChain1.GetAddressOf());
	if (FAILED(hr)) 
	{ return false; }

	// IDXGISwapChain3 に変換（GetCurrentBackBufferIndex()のために必要）
	hr = pSwapChain1.As(&m_pSwapChain);
	if (FAILED(hr)) 
	{ return false; }

	// 初期フレームインデックスを取得
	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	return true;
}

// -------------------------------------------------------------------------------
//		DescriptorPool 生成（4種類）
// -------------------------------------------------------------------------------
bool GraphicsDevice::InitDescriptorPools()
{
	// 各プールの設定をまとめて定義
	const struct
	{
		D3D12_DESCRIPTOR_HEAP_TYPE	Type;
		uint32_t					Count;
		D3D12_DESCRIPTOR_HEAP_FLAGS	Flags;
	}descs[] =
	{
		// POOL_TYPE_RES : CBV / SRV / UAV シェーダーから参照するためSHADER_VISIBLE必須
		{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,	MAX_COUNT_RES, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE},
		// POOL_TYPE_RTV : RTV				CPUからしか使わないのでNONE
		{D3D12_DESCRIPTOR_HEAP_TYPE_RTV,			MAX_COUNT_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE			},
		// POOL_TYPE_DSV : DSV				CPUからしか使わないのでNONE
		{D3D12_DESCRIPTOR_HEAP_TYPE_DSV,			MAX_COUNT_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE			},
		// POOL_TYPE_SMP : Sampler			シェーダーから参照するためSHADER_VISIBlE必須
		{D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,		MAX_COUNT_SMP, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE},
	};

	for (auto i = 0u; i < POOL_COUNT; ++i)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type			= descs[i].Type;
		desc.NumDescriptors = descs[i].Count;
		desc.Flags			= descs[i].Flags;
		desc.NodeMask		= 0;

		if (!DescriptorPool::Create(m_pDevice.Get(), &desc, &m_pPool[i])) 
		{ return false; }
	}

	return true;
}

// -------------------------------------------------------------------------------
//		ColorTarget 生成（FrameCount分）
// -------------------------------------------------------------------------------
bool GraphicsDevice::InitColorTargets()
{
	m_ColorTargets.reserve(m_Desc.FrameCount);

	for (auto i = 0u; i < m_Desc.FrameCount; ++i)
	{
		// InitFrameBackBufferはスワップチェインのバッファを取得してRTVを作る
		auto ct = std::make_unique<ColorTarget>();
		if (!ct->InitFromBackBuffer(
			m_pDevice.Get(),
			m_pPool[POOL_TYPE_RTV],
			i,
			m_pSwapChain.Get()))
		{
			return false;
		}
		m_ColorTargets.emplace_back(std::move(ct));
	}

	return true;
}

// -------------------------------------------------------------------------------
//		DepthTarget生成
// -------------------------------------------------------------------------------
bool GraphicsDevice::InitDepthTarget()
{
	if (!m_DepthTarget.Init(
		m_pDevice.Get(),
		m_pPool[POOL_TYPE_DSV],
		m_Desc.Width,
		m_Desc.Height,
		m_Desc.DepthFormat))
	{
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------
//		CommandList生成
// -------------------------------------------------------------------------------
bool GraphicsDevice::InitCommandList()
{
	// アロケータ数 = FrameCount
	// フレームごとに別のアロケータを使いまわすことで
	// GPU使用中のアロケータをリセットするエラーを防ぐ
	if (!m_CommandList.Init(
		m_pDevice.Get(),
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_Desc.FrameCount))
	{
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------
//		Fence生成
// -------------------------------------------------------------------------------
bool GraphicsDevice::InitFence()
{
	if (!m_Fence.Init(m_pDevice.Get()))
	{
		return false;
	}

	return true;
}

void GraphicsDevice::UpdateFrameIndex()
{
	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
}