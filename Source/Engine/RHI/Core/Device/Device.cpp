// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Device.h"
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
RHI::Device::Device()
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
RHI::Device::~Device() 
{ Term(); }


// -------------------------------------------------------------------------------
//		初期化
// -------------------------------------------------------------------------------
bool RHI::Device::Init(const Desc& _desc)
{
	m_Desc = _desc;

	// 各サブシステムを順番に初期化
	// 失敗した時点で終了処理を呼んで安全に抜ける
	if (!InitDevice())					{ Term(); return false; }
	if (!InitCommandQueue())			{ Term(); return false; }
	if (!InitSwapChain())				{ Term(); return false; }
	if (!InitDescriptorPools())			{ Term(); return false; }
	if (!InitColorTargets())			{ Term(); return false; }
	if (!InitDepthTarget())				{ Term(); return false; }
	if (!InitCommandList())				{ Term(); return false; }
	if (!InitFence())					{ Term(); return false; }
	if (!InitTransientResourcePool())	{ Term(); return false; }
	if (!InitDummyTexture())			{ Term(); return false; }

	return true;
}

// -------------------------------------------------------------------------------
//		終了処理
// -------------------------------------------------------------------------------
void RHI::Device::Term()
{

	// 各リソースを逆順に解放
	m_DummyTexture.Term();
	m_TransientResourcePool.ReleaseAll();
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
void RHI::Device::WaitForGPU()
{
	// m_pQueueがnullptrなら何もしない
	if (m_pQueue == nullptr) 
	{ return; }

	m_Fence.Sync(m_pQueue.Get());
}

// -------------------------------------------------------------------------------
//		バックバッファと深度バッファの作り直し
//
//	ウィンドウのサイズ変更・最大化に追従するための処理
//
//	手順
//		1. GPUの完了を待つ（実行中のコマンドが古いバッファを参照しないように）
//		2. バックバッファへの参照をすべて手放す（ResizeBuffersの前提条件）
//		3. スワップチェインを作り直す
//		4. 新しいバックバッファからRTVを作り直し、深度バッファも作り直す
//		5. フレーム番号を取り直す（ResizeBuffersで先頭に戻ることがある）
// -------------------------------------------------------------------------------
bool RHI::Device::Resize(uint32_t _width, uint32_t _height)
{
	if (m_pSwapChain == nullptr)
	{ return false; }

	// 最小化するとサイズが0になる。0では作り直せないので何もしない
	if (_width == 0 || _height == 0)
	{ return false; }

	// 同じ大きさなら作り直す必要がない
	if (_width == m_Desc.Width && _height == m_Desc.Height)
	{ return true; }

	// -------------------------------------------------------------------------------
	// 1. GPUの完了を待つ
	// -------------------------------------------------------------------------------
	WaitForGPU();

	// -------------------------------------------------------------------------------
	// 2. バックバッファへの参照をすべて手放す
	//
	//	ResizeBuffersは「そのバッファを誰も掴んでいないこと」を要求する
	//	1つでも参照が残っていると失敗する
	//	追跡表からも外しておかないと、同じアドレスに新しいバッファが載ったときに
	//	古い状態が使われてしまう
	// -------------------------------------------------------------------------------
	for (auto& colorTarget : m_ColorTargets)
	{
		if (colorTarget != nullptr)
		{
			m_ResourceStateTracker.UnRegisterResource(colorTarget->GetResource());
		}
	}
	m_ColorTargets.clear();

	m_ResourceStateTracker.UnRegisterResource(m_DepthTarget.GetResource());
	m_DepthTarget.Term();

	// -------------------------------------------------------------------------------
	// 3. スワップチェインを作り直す
	//	  生成時と同じフラグを渡さないと、以降の挙動が変わってしまう
	// -------------------------------------------------------------------------------
	const HRESULT hr = m_pSwapChain->ResizeBuffers(
		m_Desc.FrameCount,
		_width,
		_height,
		m_Desc.Format,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);

	if (FAILED(hr))
	{
		ELOG("Device::Resize() ResizeBuffers failed hr = 0x%08X", static_cast<unsigned int>(hr));
		return false;
	}

	// 以降の生成が新しい大きさを使うよう、先に反映する
	m_Desc.Width  = _width;
	m_Desc.Height = _height;

	// -------------------------------------------------------------------------------
	// 4. RTVと深度バッファを作り直す
	// -------------------------------------------------------------------------------
	if (!InitColorTargets() || !InitDepthTarget())
	{
		ELOG("Device::Resize() failed to recreate targets");
		return false;
	}

	// -------------------------------------------------------------------------------
	// 5. フレーム番号を取り直す
	// -------------------------------------------------------------------------------
	UpdateFrameIndex();

	return true;
}

// -------------------------------------------------------------------------------
//		デバッグレイヤーのメッセージをログへ出力する
// -------------------------------------------------------------------------------
void RHI::Device::FlushDebugMessages()
{
#if defined(DEBUG) || defined(_DEBUG)
	if (m_pDevice == nullptr)
	{ return; }

	ComPtr<ID3D12InfoQueue> pInfoQueue;
	if (FAILED(m_pDevice->QueryInterface(IID_PPV_ARGS(pInfoQueue.GetAddressOf()))))
	{ return; }

	const UINT64 count = pInfoQueue->GetNumStoredMessages();

	for (UINT64 i = 0; i < count; ++i)
	{
		SIZE_T length = 0;
		if (FAILED(pInfoQueue->GetMessage(i, nullptr, &length)) || length == 0)
		{ continue; }

		std::vector<uint8_t> buffer(length);
		auto* pMessage = reinterpret_cast<D3D12_MESSAGE*>(buffer.data());

		if (FAILED(pInfoQueue->GetMessage(i, pMessage, &length)))
		{ continue; }

		ELOG("[D3D12] %s", pMessage->pDescription);
	}

	pInfoQueue->ClearStoredMessages();
#endif
}

// -------------------------------------------------------------------------------
//		ゲッター
// -------------------------------------------------------------------------------
ID3D12Device*				RHI::Device::GetDevice()				const { return m_pDevice.Get(); }
ID3D12CommandQueue*			RHI::Device::GetQueue()					const { return m_pQueue.Get(); }
IDXGISwapChain3*			RHI::Device::GetSwapChain()				const { return m_pSwapChain.Get(); }
RHI::CommandList*			RHI::Device::GetCommandList()				  { return &m_CommandList; }
RHI::Fence*					RHI::Device::GetFence() 					  { return &m_Fence; }
RHI::ResourceStateTracker*	RHI::Device::GetResourceStateTracker()		  { return &m_ResourceStateTracker; }
RHI::TransientResourcePool* RHI::Device::GetTransientResourcePool()		  { return &m_TransientResourcePool; }
RHI::PipelineCache*			RHI::Device::GetPipelineCache()				  { return &m_PipelineCache; }
RHI::DescriptorPool*		RHI::Device::GetPool(POOL_TYPE _type)	const { return m_pPool[_type]; }
RHI::DepthTarget*			RHI::Device::GetDepthTarget()			const { return const_cast<RHI::DepthTarget*>(&m_DepthTarget); }
RHI::Texture* RHI::Device::GetDummyTexture()
{
	if (!m_DummyTextureInitialized)
	{
		m_DummyTextureInitialized = InitDummyTexture();
		if (!m_DummyTextureInitialized)
		{
			ELOG("Device::GetDummyTexture() : InitDummyTexture failed");
			return nullptr;
		}
	}
	return &m_DummyTexture;
}
uint32_t					RHI::Device::GetFrameIndex()			const { return m_FrameIndex; }
uint32_t					RHI::Device::GetFrameCount()			const { return m_Desc.FrameCount; }
uint32_t					RHI::Device::GetWidth()					const { return m_Desc.Width; }
uint32_t					RHI::Device::GetHeight()				const { return m_Desc.Height; }

RHI::ColorTarget* RHI::Device::GetColorTarget(uint32_t _index) const
{
	if (_index >= m_ColorTargets.size()) 
	{ return nullptr; }
	return m_ColorTargets[_index].get();
}

// -------------------------------------------------------------------------------
//		private
// -------------------------------------------------------------------------------

// -------------------------------------------------------------------------------
//		デバイス生成
// -------------------------------------------------------------------------------
bool RHI::Device::InitDevice()
{
#if defined(DEBUG) || defined(_DEBUG)
	// デバッグビルド時はデバッグレイヤーを有効化
	// デバッグレイヤーはDX12の使用法エラーをログに出力する
	{
		ComPtr<ID3D12Debug> pDebug;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(pDebug.GetAddressOf()))))
		{ pDebug->EnableDebugLayer(); }

		// GPUベースの検証まで有効にすると起動が重くなるため、
		// ここではCPU側の検証のみに留める

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

	ELOG("Error : No suitable D3D12 RHI::Device found.");
	return false;
}

// -------------------------------------------------------------------------------
//		コマンドキュー生成
// -------------------------------------------------------------------------------
bool RHI::Device::InitCommandQueue()
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
bool RHI::Device::InitSwapChain()
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
bool RHI::Device::InitDescriptorPools()
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

		if (!RHI::DescriptorPool::Create(m_pDevice.Get(), &desc, &m_pPool[i])) 
		{ return false; }
	}

	return true;
}

// -------------------------------------------------------------------------------
//		ColorTarget 生成（FrameCount分）
// -------------------------------------------------------------------------------
bool RHI::Device::InitColorTargets()
{
	m_ColorTargets.reserve(m_Desc.FrameCount);

	for (auto i = 0u; i < m_Desc.FrameCount; ++i)
	{
		// InitFrameBackBufferはスワップチェインのバッファを取得してRTVを作る
		auto ct = std::make_unique<RHI::ColorTarget>();
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

	// レンダーターゲットのリソースの登録
	for (auto& ct : m_ColorTargets)
	{
		m_ResourceStateTracker.RegisterResource(ct->GetResource(), D3D12_RESOURCE_STATE_PRESENT);
	}

	return true;
}

// -------------------------------------------------------------------------------
//		DepthTarget生成
// -------------------------------------------------------------------------------
bool RHI::Device::InitDepthTarget()
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

	// デプスターゲットのリソースの登録
	m_ResourceStateTracker.RegisterResource(m_DepthTarget.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

	return true;
}

// -------------------------------------------------------------------------------
//		CommandList生成
// -------------------------------------------------------------------------------
bool RHI::Device::InitCommandList()
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
bool RHI::Device::InitFence()
{
	if (!m_Fence.Init(m_pDevice.Get()))
	{
		return false;
	}

	return true;
}

bool RHI::Device::InitTransientResourcePool()
{
	if (!m_TransientResourcePool.Init(m_pDevice.Get()))
	{
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------
// 共有ダミーテクスチャ生成
// -------------------------------------------------------------------------------
//bool RHI::Device::InitDummyTexture()
//{
//	const uint32_t whitePixel = 0xFFFFFFFF;
//
//	D3D12_HEAP_PROPERTIES heapDefault = {};
//	heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;
//
//	D3D12_RESOURCE_DESC resDesc = {};
//	resDesc.Dimension			= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
//	resDesc.Width				= 1;
//	resDesc.Height				= 1;
//	resDesc.DepthOrArraySize	= 1;
//	resDesc.MipLevels			= 1;
//	resDesc.Format				= DXGI_FORMAT_R8G8B8A8_UNORM;
//	resDesc.SampleDesc.Count	= 1;
//	resDesc.Layout				= D3D12_TEXTURE_LAYOUT_UNKNOWN;
//	resDesc.Flags				= D3D12_RESOURCE_FLAG_NONE;
//
//	ComPtr<ID3D12Resource> pResource;
//	auto hr = m_pDevice->CreateCommittedResource(
//		&heapDefault, D3D12_HEAP_FLAG_NONE, &resDesc,
//		D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
//		IID_PPV_ARGS(pResource.GetAddressOf()));
//	if (FAILED(hr))
//	{
//		ELOG("Device::InitDummyTexture() :Resource creation failed");
//		return false;
//	}
//
//	const UINT64 uploadSize = GetRequiredIntermediateSize(pResource.Get(), 0, 1);
//	D3D12_HEAP_PROPERTIES heapUpload = {};
//	heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;
//
//	D3D12_RESOURCE_DESC uploadDesc = {};
//	uploadDesc.Dimension		= D3D12_RESOURCE_DIMENSION_BUFFER;
//	uploadDesc.Width			= uploadSize;
//	uploadDesc.Height			= 1;
//	uploadDesc.DepthOrArraySize = 1;
//	uploadDesc.MipLevels		= 1;
//	uploadDesc.Format			= DXGI_FORMAT_UNKNOWN;
//	uploadDesc.SampleDesc.Count = 1;
//	uploadDesc.Layout			= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
//
//	ComPtr<ID3D12Resource> pUpload;
//	hr = m_pDevice->CreateCommittedResource(
//		&heapUpload, D3D12_HEAP_FLAG_NONE, &uploadDesc,
//		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
//		IID_PPV_ARGS(pUpload.GetAddressOf()));
//	if (FAILED(hr)) 
//	{ return false; }
//	
//	D3D12_SUBRESOURCE_DATA subResource = {};
//	subResource.pData		= &whitePixel;
//	subResource.RowPitch	= 4;
//	subResource.SlicePitch	= 4;
//
//	auto* pCmd = m_CommandList.Reset(&m_Fence);
//	if (pCmd == nullptr) 
//	{ return false; }
//
//	UpdateSubresources(pCmd, pResource.Get(), pUpload.Get(), 0, 0, 1, &subResource);
//
//	D3D12_RESOURCE_BARRIER barrier = {};
//	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
//	barrier.Transition.pResource = pResource.Get();
//	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
//	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
//	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
//	pCmd->ResourceBarrier(1, &barrier);
//	pCmd->Close();
//
//	ID3D12CommandList* ppLists[] = { pCmd };
//	m_pQueue->ExecuteCommandLists(1, ppLists);
//
//	const auto fenceValue = m_Fence.Signal(m_pQueue.Get());
//	m_CommandList.RecordFenceValue(fenceValue);
//	m_Fence.WaitForValue(fenceValue);
//
//	m_DummyTextureInitialized = true;
//
//	return m_DummyTexture.InitFromResource(
//		m_pDevice.Get(), m_pPool[POOL_TYPE_RES], pResource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM);
//}

// Device.cpp: InitDummyTexture() の修正版
bool RHI::Device::InitDummyTexture()
{
	const uint32_t whitePixel = 0xFFFFFFFF;

	D3D12_HEAP_PROPERTIES heapDefault = {};
	heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Width = 1;
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ComPtr<ID3D12Resource> pResource;
	auto hr = m_pDevice->CreateCommittedResource(
		&heapDefault, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		IID_PPV_ARGS(pResource.GetAddressOf()));
	if (FAILED(hr))
	{
		ELOG("Device::InitDummyTexture() :Resource creation failed");
		return false;
	}

	const UINT64 uploadSize = GetRequiredIntermediateSize(pResource.Get(), 0, 1);
	D3D12_HEAP_PROPERTIES heapUpload = {};
	heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC uploadDesc = {};
	uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadDesc.Width = uploadSize;
	uploadDesc.Height = 1;
	uploadDesc.DepthOrArraySize = 1;
	uploadDesc.MipLevels = 1;
	uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
	uploadDesc.SampleDesc.Count = 1;
	uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ComPtr<ID3D12Resource> pUpload;
	hr = m_pDevice->CreateCommittedResource(
		&heapUpload, D3D12_HEAP_FLAG_NONE, &uploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(pUpload.GetAddressOf()));
	if (FAILED(hr))
	{
		return false;
	}

	D3D12_SUBRESOURCE_DATA subResource = {};
	subResource.pData = &whitePixel;
	subResource.RowPitch = 4;
	subResource.SlicePitch = 4;

	// -------------------------------------------------------------------------------
	// 修正: 共有の m_CommandList / m_Fence を横取りせず、
	// この初期化専用の使い捨てコマンドアロケータ/リスト/フェンスをその場で作る。
	// フレーム描画側の状態(記録中かどうか)に一切依存しなくなる。
	// -------------------------------------------------------------------------------
	ComPtr<ID3D12CommandAllocator>		pAlloc;
	ComPtr<ID3D12GraphicsCommandList>	pCmd;
	ComPtr<ID3D12Fence>					pLocalFence;

	hr = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(pAlloc.GetAddressOf()));
	if (FAILED(hr)) { return false; }

	hr = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pAlloc.Get(), nullptr, IID_PPV_ARGS(pCmd.GetAddressOf()));
	if (FAILED(hr)) { return false; }

	UpdateSubresources(pCmd.Get(), pResource.Get(), pUpload.Get(), 0, 0, 1, &subResource);

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = pResource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	pCmd->ResourceBarrier(1, &barrier);
	pCmd->Close();

	ID3D12CommandList* ppLists[] = { pCmd.Get() };
	m_pQueue->ExecuteCommandLists(1, ppLists);

	hr = m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(pLocalFence.GetAddressOf()));
	if (FAILED(hr)) { return false; }

	HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!hEvent) { return false; }

	m_pQueue->Signal(pLocalFence.Get(), 1);
	pLocalFence->SetEventOnCompletion(1, hEvent);
	WaitForSingleObject(hEvent, INFINITE);
	CloseHandle(hEvent);

	return m_DummyTexture.InitFromResource(
		m_pDevice.Get(), m_pPool[POOL_TYPE_RES], pResource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM);
}

void RHI::Device::UpdateFrameIndex()
{
	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
}
