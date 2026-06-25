// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Application.h"
#include "../Framework/Utility/FileUtil/FileUtil.h"

namespace /* anonymous */{
	const auto ClassName = TEXT("SampleWindowClass");	// ウィンドウクラス名
}	// namespace /* anonymous */

// -------------------------------------------------------------------------------
// コンストラクタ
// -------------------------------------------------------------------------------
Application::Application(uint32_t width, uint32_t height)
	: m_hInst(nullptr)
	, m_hWnd(nullptr)
	, m_Width(width)
	, m_Height(height)
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
// デストラクタ
// -------------------------------------------------------------------------------
Application::~Application()
{ /* DO_NOTHING */ }

// 実行
void Application::Run()
{
	if(InitApplication())
	{
		MainLoop();
	}

	TermApplication();
}

// -------------------------------------------------------------------------------
// 初期化処理
// -------------------------------------------------------------------------------
bool Application::InitApplication()
{
	// ウィンドウの初期化
	if (!InitWnd())
	{
		return false;
	}

	// Direct3D 12の初期化
	if(!InitD3D())
	{
		return false;
	}

	if(!OnInit())
	{
		return false;
	}

	// 正常終了
	return true;
}

// -------------------------------------------------------------------------------
// 終了処理
// -------------------------------------------------------------------------------
void Application::TermApplication()
{
	// ウィンドウの終了処理
	TermD3D();
	TermWnd();
}

// -------------------------------------------------------------------------------
// ウィンドウの初期化処理
// -------------------------------------------------------------------------------
bool Application::InitWnd()
{
	// インスタンスハンドルを取得
	auto hInst = GetModuleHandle(nullptr);
	if(hInst == nullptr)
	{
		return false;
	}

	// ウィンドウの設定
	WNDCLASSEX wc = {};
	wc.cbSize			= sizeof(WNDCLASSEX);
	wc.style			= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc		= WndProc;
	wc.hIcon			= LoadIcon(hInst, IDI_APPLICATION);
	wc.hCursor			= LoadCursor(hInst, IDC_ARROW);
	wc.hbrBackground	= GetSysColorBrush(COLOR_BACKGROUND);
	wc.lpszMenuName		= nullptr;
	wc.lpszClassName	= ClassName;
	wc.hIconSm			= LoadIcon(hInst, IDI_APPLICATION);

	// ウィンドウの登録
	if (!RegisterClassEx(&wc))
	{
		return false;
	}

	// インスタンスハンドル設定
	m_hInst = hInst;

	// ウィンドウのサイズを設定
	RECT rc = {};
	rc.right = static_cast<LONG>(m_Width);
	rc.bottom = static_cast<LONG>(m_Height);

	// ウィンドウサイズを調整
	auto style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
	AdjustWindowRect(&rc, style, FALSE);

	// ウィンドウを生成
	m_hWnd = CreateWindowEx(
		0,
		ClassName,
		TEXT("Sample"),
		style,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr,
		nullptr,
		m_hInst,
		nullptr);

	if(m_hWnd == nullptr)
	{
		return false;
	}

	// ウィンドウを表示
	ShowWindow(m_hWnd, SW_SHOWNORMAL);

	// ウィンドウを更新
	UpdateWindow(m_hWnd);

	// ウィンドウにフォーカスを設定
	SetFocus(m_hWnd);

	// 正常終了
	return true;
}

// -------------------------------------------------------------------------------
// ウィンドウの終了処理
// -------------------------------------------------------------------------------
void Application::TermWnd()
{
	// ウィンドウの登録を解除
	if(m_hInst != nullptr)
	{
		UnregisterClass(ClassName, m_hInst);
	}

	m_hInst = nullptr;
	m_hWnd = nullptr;
}

// -------------------------------------------------------------------------------
// ウィンドウプロシージャ
// -------------------------------------------------------------------------------
LRESULT CALLBACK Application::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg)
	{
		case WM_DESTROY:
			{ PostQuitMessage(0); }
			break;

		default:
			{/* DO_NOTHING */ }
			break;
	}

	return DefWindowProc(hWnd, msg, wp, lp);
}

// -------------------------------------------------------------------------------
// メインループ
// -------------------------------------------------------------------------------
void Application::MainLoop()
{
	MSG msg = {};

	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) == TRUE)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Render();
		}
	}
}

// -------------------------------------------------------------------------------
// Direct3Dの初期化処理
// -------------------------------------------------------------------------------
bool Application::InitD3D()
{
#if defined(DEBUG) || defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debug;
		auto hr = D3D12GetDebugInterface(IID_PPV_ARGS(debug.GetAddressOf()));

		// デバッグレイヤーを有効化
		if (SUCCEEDED(hr)) 
		{ debug->EnableDebugLayer(); }
	}
#endif

	// デバイスの生成
	auto hr = D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(m_pDevice.GetAddressOf()));
	if(FAILED(hr))
	{
		return false;
	}

	// コマンドキューの生成
	{
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Type		= D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc.Priority	= D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags		= D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask	= 0;

		hr = m_pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_pQueue));
		if (FAILED(hr))
		{
			return false;
		}
	}

	// スワップチェインの生成
	{
		// DXGIファクトリーの生成
		IDXGIFactory4* pFactory = nullptr;
		hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
		if(FAILED(hr))
		{
			return false;
		}

		// スワップチェインの設定
		DXGI_SWAP_CHAIN_DESC desc = {};
		desc.BufferDesc.Width					= m_Width;
		desc.BufferDesc.Height					= m_Height;
		desc.BufferDesc.RefreshRate.Numerator	= 60;
		desc.BufferDesc.RefreshRate.Denominator = 1;
		desc.BufferDesc.ScanlineOrdering		= DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		desc.BufferDesc.Scaling					= DXGI_MODE_SCALING_UNSPECIFIED;
		desc.BufferDesc.Format					= DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count					= 1;
		desc.SampleDesc.Quality					= 0;
		desc.BufferUsage						= DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount						= FrameCount;
		desc.OutputWindow						= m_hWnd;
		desc.Windowed							= TRUE;
		desc.SwapEffect							= DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.Flags								= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		// スワップチェインの生成
		IDXGISwapChain* pSwapChain = nullptr;
		hr = pFactory->CreateSwapChain(m_pQueue.Get(), &desc, &pSwapChain);
		if (FAILED(hr))
		{
			pFactory->Release();
			return false;
		}

		// IDXGISwapChain3 を取得
		hr = pSwapChain->QueryInterface(IID_PPV_ARGS(&m_pSwapChain));
		if (FAILED(hr))
		{
			pFactory->Release();
			pSwapChain->Release();
			return false;
		}

		// バックバッファ番号を取得
		m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

		// 不要になったので解放
		pFactory->Release();
		pSwapChain->Release();
	}

	// コマンドアロケータの生成
	{
		for (auto i = 0u; i < FrameCount; ++i)
		{
			hr = m_pDevice->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(&m_pCmdAllocator[i]));
			if (FAILED(hr))
			{ return false; }
		}
	}

	// コマンドリストの生成
	{
		hr = m_pDevice->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_pCmdAllocator[m_FrameIndex].Get(),
			nullptr,
			IID_PPV_ARGS(&m_pCmdList));
		if (FAILED(hr)) 
		{ return false; }
	}

	// レンダーターゲットビューの生成
	{
		// ディスクリプタヒープの設定
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = FrameCount;
		desc.Type			= D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.Flags			= D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask		= 0;

		// ディスクリプタヒープを生成
		hr = m_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pHeapRTV));
		if (FAILED(hr)) 
		{ return false; }

		auto handle			= m_pHeapRTV->GetCPUDescriptorHandleForHeapStart();
		auto incrementSize	= m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		for (auto i = 0u; i < FrameCount; ++i)
		{
			hr = m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pColorBuffer[i]));
			if (FAILED(hr)) 
			{ return false; }

			D3D12_RENDER_TARGET_VIEW_DESC viewDesc = {};;
			viewDesc.Format					= DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			viewDesc.ViewDimension			= D3D12_RTV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MipSlice		= 0;
			viewDesc.Texture2D.PlaneSlice	= 0;

			// レンダーターゲットビューの生成
			m_pDevice->CreateRenderTargetView(m_pColorBuffer[i].Get(), &viewDesc, handle);

			m_HandleRTV[i] = handle;
			handle.ptr += incrementSize;
		}
	}

	// フェンスの生成
	{
		// フェンスカウンターをリセット
		for (auto i = 0u; i < FrameCount; ++i) 
		{ m_FenceCounter[i] = 0; }

		// フェンスの生成
		hr = m_pDevice->CreateFence(
			m_FenceCounter[m_FrameIndex],
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(&m_pFence));
		if (FAILED(hr)) 
		{ return false; }

		m_FenceCounter[m_FrameIndex]++;

		// イベントの生成
		m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_FenceEvent == nullptr) 
		{ return false; }
	}

	// 深度ステンシルバッファの生成
	{
		D3D12_HEAP_PROPERTIES prop = {};
		prop.Type					= D3D12_HEAP_TYPE_DEFAULT;
		prop.CPUPageProperty		= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference	= D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask		= 1;
		prop.VisibleNodeMask		= 1;

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension			= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.Alignment			= 0;
		resDesc.Width				= m_Width;
		resDesc.Height				= m_Height;
		resDesc.DepthOrArraySize	= 1;
		resDesc.MipLevels			= 1;
		resDesc.Format				= DXGI_FORMAT_D32_FLOAT;
		resDesc.SampleDesc.Count	= 1;
		resDesc.Layout				= D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resDesc.Flags				= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE clearValue;
		clearValue.Format				= DXGI_FORMAT_D32_FLOAT;
		clearValue.DepthStencil.Depth	= 1.0f;
		clearValue.DepthStencil.Stencil = 0;

		hr = m_pDevice->CreateCommittedResource(
			&prop,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearValue,
			IID_PPV_ARGS(m_pDepthBuffer.GetAddressOf())); 
		if (FAILED(hr)) 
		{ return false; }

		// ディスクリプタヒープの設定
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = 1;
		heapDesc.Type			= D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		heapDesc.Flags			= D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heapDesc.NodeMask		= 0;

		hr = m_pDevice->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_pHeapDSV.GetAddressOf()));
		if (FAILED(hr)) 
		{ return false; }

		auto handle = m_pHeapDSV->GetCPUDescriptorHandleForHeapStart();
		auto incrementSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
		viewDesc.Format				= DXGI_FORMAT_D32_FLOAT;
		viewDesc.ViewDimension		= D3D12_DSV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipSlice = 0;

		m_pDevice->CreateDepthStencilView(m_pDepthBuffer.Get(), &viewDesc, handle);

		m_HandleDSV = handle;
	}

	// コマンドリストを閉じる
	m_pCmdList->Close();

	return true;
}

bool Application::OnInit()
{
	// メッシュをロード
	{
		std::wstring path;
		if (!SearchFilePath(L"Assets/teapot/teapot.obj", path))
		{
			return false;
		}

		if (!LoadMesh(path.c_str(), m_Meshes, m_Materials))
		{
			return false;
		}

		// このサンプルでは、メッシュが1つのみとする
		assert(m_Meshes.size() == 1);
	}

	// 頂点バッファの生成
	{
		// 頂点データ
		/*Vertex vertices[] = {
			{DirectX::XMFLOAT3(-1.0f,1.0f,0.0f), DirectX::XMFLOAT2(0.0f,0.0f), DirectX::XMFLOAT4(1.0f,0.0f,0.0f,1.0f)},
			{DirectX::XMFLOAT3(1.0f,1.0f,0.0f), DirectX::XMFLOAT2(1.0f,0.0f), DirectX::XMFLOAT4(0.0f,1.0f,0.0f,1.0f)},
			{DirectX::XMFLOAT3(1.0f,-1.0f,0.0f), DirectX::XMFLOAT2(1.0f,1.0f), DirectX::XMFLOAT4(0.0f,0.0f,1.0f,1.0f)},
			{DirectX::XMFLOAT3(-1.0f, -1.0f,0.0f), DirectX::XMFLOAT2(0.0f,1.0f), DirectX::XMFLOAT4(1.0f,0.0f,1.0f,1.0f)},
		};*/

		auto size = sizeof(MeshVertex) * m_Meshes[0].Vertices.size();
		auto vertices = m_Meshes[0].Vertices.data();


		// ヒーププロパティ
		D3D12_HEAP_PROPERTIES prop = {};
		prop.Type					= D3D12_HEAP_TYPE_UPLOAD;
		prop.CPUPageProperty		= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference	= D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask		= 1;
		prop.VisibleNodeMask		= 1;

		// リソースの設定
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment			= 0;
		desc.Width				= size;
		desc.Height				= 1;
		desc.DepthOrArraySize	= 1;
		desc.MipLevels			= 1;
		desc.Format				= DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count	= 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout				= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags				= D3D12_RESOURCE_FLAG_NONE;

		// リソースを生成
		auto hr = m_pDevice->CreateCommittedResource(
			&prop,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_pVB.GetAddressOf()));
		if (FAILED(hr))
		{
			return false;
		}

		// マッピングする
		void* ptr = nullptr;
		hr = m_pVB->Map(0, nullptr, &ptr);
		if (FAILED(hr))
		{
			return false;
		}

		// 頂点データをマッピング先に設定
		memcpy(ptr, vertices, size);

		// マッピング解除
		m_pVB->Unmap(0, nullptr);

		// 頂点バッファビューの設定
		m_VBV.BufferLocation = m_pVB->GetGPUVirtualAddress();
		m_VBV.SizeInBytes = static_cast<UINT>(size);
		m_VBV.StrideInBytes = static_cast<UINT>(sizeof(MeshVertex));
	}

	// インデックスバッファの生成
	{
		//uint32_t indices[] = { 0,1,2,0,2,3 };
		auto size = sizeof(uint32_t) * m_Meshes[0].Indices.size();
		auto indices = m_Meshes[0].Indices.data();

		// ヒーププロパティ
		D3D12_HEAP_PROPERTIES prop = {};
		prop.Type = D3D12_HEAP_TYPE_UPLOAD;
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask = 1;
		prop.VisibleNodeMask = 1;

		// リソースの設定
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = size;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		// リソースを生成
		auto hr = m_pDevice->CreateCommittedResource(
			&prop,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_pIB.GetAddressOf()));
		if (FAILED(hr))
		{
			return false;
		}

		// マッピングする
		void* ptr = nullptr;
		hr = m_pIB->Map(0, nullptr, &ptr);
		if (FAILED(hr))
		{
			return false;
		}

		// インデックスデータをマッピング先に設定
		memcpy(ptr, indices, size);

		// マッピング解除
		m_pIB->Unmap(0, nullptr);

		// インデックスバッファビューの設定
		m_IBV.BufferLocation = m_pIB->GetGPUVirtualAddress();
		m_IBV.Format = DXGI_FORMAT_R32_UINT;
		m_IBV.SizeInBytes = static_cast<UINT>(size);
	}

	// 定数バッファ用ディスクリプタヒープの生成
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 2 * FrameCount + 16;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		desc.NodeMask = 0;

		auto hr = m_pDevice->CreateDescriptorHeap(
			&desc,
			IID_PPV_ARGS(m_pHeapCBV.GetAddressOf()));
		if (FAILED(hr))
		{
			return false;
		}
	}

	// 定数バッファの生成
	{
		// ヒーププロパティ
		D3D12_HEAP_PROPERTIES prop = {};
		prop.Type = D3D12_HEAP_TYPE_UPLOAD;
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask = 1;
		prop.VisibleNodeMask = 1;

		// リソースの設定
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = sizeof(Transform);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		auto incrementSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		for (auto i = 0; i < FrameCount * 2; ++i)
		{
			// リソース生成
			auto hr = m_pDevice->CreateCommittedResource(
				&prop,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(m_pCB[i].GetAddressOf()));
			if (FAILED(hr))
			{
				return false;
			}

			auto address	= m_pCB[i]->GetGPUVirtualAddress();
			auto handleCPU	= m_pHeapCBV->GetCPUDescriptorHandleForHeapStart();
			auto handleGPU	= m_pHeapCBV->GetGPUDescriptorHandleForHeapStart();

			handleCPU.ptr += incrementSize * i;
			handleGPU.ptr += incrementSize * i;

			// 定数バッファビューの設定
			m_CBV[i].HandleCPU = handleCPU;
			m_CBV[i].HandleGPU = handleGPU;
			m_CBV[i].Desc.BufferLocation = address;
			m_CBV[i].Desc.SizeInBytes = sizeof(Transform);

			// 定数バッファビューを生成
			m_pDevice->CreateConstantBufferView(&m_CBV[i].Desc, handleCPU);

			// マッピング
			hr = m_pCB[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_CBV[i].pBuffer));
			if (FAILED(hr))
			{
				return false;
			}

			auto eyePos = DirectX::XMVectorSet(0.0f, 0.0f, 5.0f, 0.0f);
			auto targetPos = DirectX::XMVectorZero();
			auto upward = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

			auto fovY = DirectX::XMConvertToRadians(37.5f);
			auto aspect = static_cast<float>(m_Width) / static_cast<float>(m_Height);

			// 変換行列の設定
			m_CBV[i].pBuffer->World = DirectX::XMMatrixIdentity();
			m_CBV[i].pBuffer->View = DirectX::XMMatrixLookAtRH(eyePos, targetPos, upward);
			m_CBV[i].pBuffer->Proj = DirectX::XMMatrixPerspectiveFovRH(fovY, aspect, 1.0f, 1000.0f);
		}
	}

	// ルートシグニチャの生成
	{
		auto flag = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
		flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
		flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		// DescriptorRange : SRV 1枚（t0）
		D3D12_DESCRIPTOR_RANGE srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = 1;
		srvRange.BaseShaderRegister = 0;
		srvRange.RegisterSpace = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// ルートパラメータの設定
		// RootParameter[0] : CBV
		D3D12_ROOT_PARAMETER params[2] = {};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[0].Descriptor.ShaderRegister = 0;
		params[0].Descriptor.RegisterSpace = 0;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		// RootParameter[1] : DescriptorTable SRV
		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].DescriptorTable.NumDescriptorRanges = 1;
		params[1].DescriptorTable.pDescriptorRanges = &srvRange;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// StaticSampler
		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// ルートシグニチャの設定
		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = 2;
		desc.NumStaticSamplers = 1;
		desc.pParameters = params;
		desc.pStaticSamplers = &sampler;
		desc.Flags = flag;

		ComPtr<ID3DBlob> pBlob;
		ComPtr<ID3DBlob> pErrorBlob;

		// シリアライズ
		auto hr = D3D12SerializeRootSignature(
			&desc,
			D3D_ROOT_SIGNATURE_VERSION_1_0,
			pBlob.GetAddressOf(),
			pErrorBlob.GetAddressOf());
		if (FAILED(hr))
		{
			return false;
		}

		// ルートシグニチャを生成
		hr = m_pDevice->CreateRootSignature(
			0,
			pBlob->GetBufferPointer(),
			pBlob->GetBufferSize(),
			IID_PPV_ARGS(m_pRootSignature.GetAddressOf()));
		if (FAILED(hr))
		{
			return false;
		}
	}

	// パイプラインステートの生成
	{
		// 入力レイアウトの設定
		D3D12_INPUT_ELEMENT_DESC elements[4];
		elements[0].SemanticName = "POSITION";
		elements[0].SemanticIndex = 0;
		elements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		elements[0].InputSlot = 0;
		elements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		elements[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		elements[0].InstanceDataStepRate = 0;

		elements[1].SemanticName = "NORMAL";
		elements[1].SemanticIndex = 0;
		elements[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		elements[1].InputSlot = 0;
		elements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		elements[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		elements[1].InstanceDataStepRate = 0;

		elements[2].SemanticName = "TEXCOORD";
		elements[2].SemanticIndex = 0;
		elements[2].Format = DXGI_FORMAT_R32G32_FLOAT;
		elements[2].InputSlot = 0;
		elements[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		elements[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		elements[2].InstanceDataStepRate = 0;

		elements[3].SemanticName = "TANGENT";
		elements[3].SemanticIndex = 0;
		elements[3].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		elements[3].InputSlot = 0;
		elements[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		elements[3].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		elements[3].InstanceDataStepRate = 0;

		// ラスタライザーステートの設定
		D3D12_RASTERIZER_DESC descRS;
		descRS.FillMode = D3D12_FILL_MODE_SOLID;
		descRS.CullMode = D3D12_CULL_MODE_NONE;
		descRS.FrontCounterClockwise = FALSE;
		descRS.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		descRS.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		descRS.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		descRS.DepthClipEnable = FALSE;
		descRS.MultisampleEnable = FALSE;
		descRS.AntialiasedLineEnable = FALSE;
		descRS.ForcedSampleCount = 0;
		descRS.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		// レンダーターゲットのブレンド設定
		D3D12_RENDER_TARGET_BLEND_DESC descRTBS = {
			FALSE,FALSE,
			D3D12_BLEND_ONE,D3D12_BLEND_ZERO,D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE,D3D12_BLEND_ZERO,D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL
		};

		// ブレンドステートの設定
		D3D12_BLEND_DESC descBS;
		descBS.AlphaToCoverageEnable = FALSE;
		descBS.IndependentBlendEnable = FALSE;
		for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		{
			descBS.RenderTarget[i] = descRTBS;
		}

		// 深度ステンシルステートの設定
		D3D12_DEPTH_STENCIL_DESC descDSS = {};
		descDSS.DepthEnable = TRUE;
		descDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		descDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		descDSS.StencilEnable = FALSE;

		ComPtr<ID3DBlob> pVSBlob;
		ComPtr<ID3DBlob> pPSBlob;

		// 頂点シェーダ読み込み
		auto hr = D3DReadFileToBlob(L"SimpleVS.cso", pVSBlob.GetAddressOf());
		if (FAILED(hr))
		{
			return false;
		}

		// ピクセルシェーダ読み込み
		hr = D3DReadFileToBlob(L"SimplePS.cso", pPSBlob.GetAddressOf());
		if (FAILED(hr))
		{
			return false;
		}

		// パイプラインステートの設定
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.InputLayout = MeshVertex::InputLayout;
		desc.pRootSignature = m_pRootSignature.Get();
		desc.VS = { pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize() };
		desc.PS = { pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize() };
		desc.RasterizerState = descRS;
		desc.BlendState = descBS;
		desc.DepthStencilState = descDSS;
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		// パイプラインステートを生成
		hr = m_pDevice->CreateGraphicsPipelineState(
			&desc,
			IID_PPV_ARGS(m_pPSO.GetAddressOf()));
		if (FAILED(hr))
		{
			return false;
		}
	}

	// ビューポートとシザー矩形の設定
	{
		m_Viewport.TopLeftX = 0;
		m_Viewport.TopLeftY = 0;
		m_Viewport.Width = static_cast<float>(m_Width);
		m_Viewport.Height = static_cast<float>(m_Height);
		m_Viewport.MinDepth = 0.0f;
		m_Viewport.MaxDepth = 1.0f;

		m_Scissor.left = 0;
		m_Scissor.right = m_Width;
		m_Scissor.top = 0;
		m_Scissor.bottom = m_Height;
	}

	// TextureManagerの初期化
	// ※CBVヒープ（m_pHeapCBV）にSRVスロットを同居させる場合
	// （CBV * 4 が先に入っているのでBaseSlot = FrameCount * 2からSRVを開始）
	TextureManager::InitDesc texDesc = {};
	texDesc.pDevice = m_pDevice.Get();
	texDesc.pQueue = m_pQueue.Get();
	//texDesc.pHeapSRV = m_pHeapCBV.Get();	// CBV_SRV_UAV兼用ヒープ
	//texDesc.pPool = 
	//texDesc.BaseSlot = FrameCount * 2;	// CBV分のスロットを避ける
	texDesc.MaxTextures = 64;				// 最大64枚

	if (!m_TextureManager.Init(texDesc))
	{
		return false;
	}

	// テクスチャのロード
	if (!m_TextureManager.Load(L"C:/DX12NextPlay/DX12Framework/Assets/Textures/Test/Keru.png", m_TexSlot))
	{
		return false;
	}


	return true;
}


// -------------------------------------------------------------------------------
// Direc3Dの終了処理
// -------------------------------------------------------------------------------
void Application::TermD3D()
{
	// GPU処理の完了を待機
	WaitGpu();

	// イベントは着
	if (m_FenceEvent != nullptr)
	{
		CloseHandle(m_FenceEvent);
		m_FenceEvent = nullptr;
	}

	// フェンス破棄
	m_pFence.Reset();

	// レンダーターゲットビューの破棄
	m_pHeapRTV.Reset();
	for(auto i = 0u; i < FrameCount; ++i)
	{
		m_pColorBuffer[i].Reset();
	}

	// 深度ステンシルビューの破棄
	m_pHeapDSV.Reset();
	m_pDepthBuffer.Reset();

	m_pPSO.Reset();
	m_pRootSignature.Reset();

	for (int i = 0; i < FrameCount * 2; i++)
	{
		m_pCB[i].Reset();
	}

	m_pHeapCBV.Reset();

	m_pVB.Reset();
	m_pIB.Reset();

	// コマンドリストの破棄
	m_pCmdList.Reset();

	// コマンドアロケータの破棄
	for (auto i = 0u; i < FrameCount * 2; ++i)
	{
		m_pCmdAllocator[i].Reset();
	}

	// スワップチェインの破棄
	m_pSwapChain.Reset();

	// コマンドキューの破棄
	m_pQueue.Reset();

	// デバイスの破棄
	m_pDevice.Reset();
}

// -------------------------------------------------------------------------------
// 描画処理
// -------------------------------------------------------------------------------
void Application::Render()
{
	// 更新処理
	{
		m_RotateAngle += 0.025f;
		m_CBV[m_FrameIndex * 2 + 0].pBuffer->World = 
			DirectX::XMMatrixRotationY(m_RotateAngle + DirectX::XMConvertToRadians(45.0f));
		m_CBV[m_FrameIndex * 2 + 1].pBuffer->World =
			DirectX::XMMatrixRotationY(m_RotateAngle) * DirectX::XMMatrixScaling(2.0f, 0.5f, 1.0f);
	}

	// コマンドの記録を開始
	m_pCmdAllocator[m_FrameIndex]->Reset();
	m_pCmdList->Reset(m_pCmdAllocator[m_FrameIndex].Get(), nullptr);

	// リソースバリアの設定
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags					= D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource	= m_pColorBuffer[m_FrameIndex].Get();
	barrier.Transition.StateBefore	= D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter	= D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	// リソースバリア
	m_pCmdList->ResourceBarrier(1, &barrier);

	// レンダーターゲットの設定
	m_pCmdList->OMSetRenderTargets(1, &m_HandleRTV[m_FrameIndex], FALSE, &m_HandleDSV);

	// クリアカラーの設定
	float clearColor[] = { 0.25f,0.25f,0.25f,1.0f };

	// レンダーターゲットビューをクリア
	m_pCmdList->ClearRenderTargetView(m_HandleRTV[m_FrameIndex], clearColor, 0, nullptr);

	// 深度ステンシルビューをクリア
	m_pCmdList->ClearDepthStencilView(m_HandleDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// 描画処理
	{
		m_pCmdList->SetGraphicsRootSignature(m_pRootSignature.Get());
		m_pCmdList->SetDescriptorHeaps(1, m_pHeapCBV.GetAddressOf());
		//m_pCmdList->SetGraphicsRootConstantBufferView(0, m_CBV[m_FrameIndex].Desc.BufferLocation);
		m_pCmdList->SetPipelineState(m_pPSO.Get());

		m_pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pCmdList->IASetVertexBuffers(0, 1, &m_VBV);
		m_pCmdList->IASetIndexBuffer(&m_IBV);
		m_pCmdList->RSSetViewports(1, &m_Viewport);
		m_pCmdList->RSSetScissorRects(1, &m_Scissor);

		// 手前側の三角形を描画
		m_pCmdList->SetGraphicsRootConstantBufferView(0, m_CBV[m_FrameIndex * 2 + 0].Desc.BufferLocation);
		const auto* tex = m_TextureManager.GetBySlot(m_TexSlot);
		if (tex)
		{
			m_pCmdList->SetGraphicsRootDescriptorTable(
				1,
				tex->GetHandleGPU());
		}
		auto count = static_cast<uint32_t>(m_Meshes[0].Indices.size());
		m_pCmdList->DrawIndexedInstanced(count, 1, 0, 0, 0);

		//// 奥川の三角形を描画
		//m_pCmdList->SetGraphicsRootConstantBufferView(0, m_CBV[m_FrameIndex * 2 + 1].Desc.BufferLocation);
		//m_pCmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
	}

	// リソースバリアの設定
	barrier.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags					= D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource	= m_pColorBuffer[m_FrameIndex].Get();
	barrier.Transition.StateBefore	= D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter	= D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	// リソースバリア
	m_pCmdList->ResourceBarrier(1, &barrier);

	// コマンドの記録を終了
	m_pCmdList->Close();

	// コマンドを実行
	ID3D12CommandList* ppCmdLists[] = { m_pCmdList.Get() };
	m_pQueue->ExecuteCommandLists(1, ppCmdLists);

	// 画面に表示
	Present(1);
}

// -------------------------------------------------------------------------------
// GPUの処理完了を待機
// -------------------------------------------------------------------------------
void Application::WaitGpu()
{
	assert(m_pQueue.Get() != nullptr);
	assert(m_pFence.Get() != nullptr);
	assert(m_FenceEvent != nullptr);

	// シグナル処理
	m_pQueue->Signal(m_pFence.Get(), m_FenceCounter[m_FrameIndex]);

	// 完了時にイベントを設定する
	m_pFence->SetEventOnCompletion(m_FenceCounter[m_FrameIndex], m_FenceEvent);

	// 待機処理
	WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);

	// カウンターを増やす
	m_FenceCounter[m_FrameIndex]++;
}

// -------------------------------------------------------------------------------
// 画面に表示し、次のフレームの準備を行う
// -------------------------------------------------------------------------------
void Application::Present(uint32_t interval)
{
	// 画面に表示
	m_pSwapChain->Present(interval, 0);

	// シグナル処理
	const auto currentValue = m_FenceCounter[m_FrameIndex];
	m_pQueue->Signal(m_pFence.Get(), currentValue);

	// バックバッファ番号を更新
	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	// 次のフレームの描画準備がまだであれば待機する
	if (m_pFence->GetCompletedValue() < m_FenceCounter[m_FrameIndex])
	{
		m_pFence->SetEventOnCompletion(m_FenceCounter[m_FrameIndex], m_FenceEvent);
		WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
	}

	// 次のフレームのフェンスカウンターを増やす
	m_FenceCounter[m_FrameIndex] = currentValue + 1;
}

