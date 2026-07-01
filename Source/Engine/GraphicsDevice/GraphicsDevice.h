#pragma once

// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/Pool/DescriptorPool/DescriptorPool.h>
#include <Engine/Target/ColorTarget/ColorTarget.h>
#include <Engine/Target/DepthTarget/DepthTarget.h>
#include <Engine/CommandList/CommandList.h>
#include <Engine/Fence/Fence.h>

// -------------------------------------------------------------------------------
// GraphicsDevice クラス
// 
// 概要 : 
//	DX12の描画基盤を一括管理するクラス
//	デバイス・スワップチェーン・コマンドキュー・フェンス・
//	DescriptorPool・ColorTarget・DepthTargetを持ち、
//	フレームループに必要なBeginFrame / EndFrame / Presentを提供する
// 
// DescriptorPoolの種類 : 
//	POOL_TYPE_RES : CBV / SRV / UAV（シェーダーリソース全般）
//	POOL_TYPE_RTV : RenderTargetView
//	POOL_TYPE_DSV : DepthStencilView
//	POOL_TYPE_SMP : Sampler
// 
// 使い方 : 
//	GraphicsDevice::Desc desc;
//	desc.hWnd		= hWnd;
//	desc.Width		= 1280;
//	desc.Height		= 720;
//	desc.Format		= DXGI_FORMAT_R8G8B8A8_UNORM;
//	desc.FrameCount = 2;
// 
//	GraphicsDevice device;
//	device.Init(desc);
// 
//	// メインループ
//	auto* pCmd = device.BeginFrame();
//	// ... 描画命令 ...
//	device.EndFrame();
//	device.Present(1);
// 
//	device.Term();
// -------------------------------------------------------------------------------
class GraphicsDevice
{
public:

	D3D_FEATURE_LEVEL levels[5] =
	{
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	// -------------------------------------------------------------------------------
	// DescriptorPool の種類
	// -------------------------------------------------------------------------------
	enum POOL_TYPE : uint32_t
	{
		POOL_TYPE_RES = 0,	// CBV / SRV / UAV
		POOL_TYPE_RTV,		// RenderTargetView
		POOL_TYPE_DSV,		// DepthStencilView
		POOL_TYPE_SMP,		// Sampler
		POOL_COUNT,
	};

	// -------------------------------------------------------------------------------
	// 各PoolTypeごとのデフォルト最大ハンドル数
	// -------------------------------------------------------------------------------
	static constexpr uint32_t MAX_COUNT_RES = 512;
	static constexpr uint32_t MAX_COUNT_RTV = 512;
	static constexpr uint32_t MAX_COUNT_DSV = 512;
	static constexpr uint32_t MAX_COUNT_SMP = 256;

	// -------------------------------------------------------------------------------
	// 初期化パラメータ
	// -------------------------------------------------------------------------------
	struct Desc
	{
		HWND		hWnd		= nullptr;						// ウィンドウハンドル
		uint32_t	Width		= 1280;							// バックバッファ横幅
		uint32_t	Height		= 720;							// バックバッファ縦幅
		DXGI_FORMAT Format		= DXGI_FORMAT_R8G8B8A8_UNORM;	// バックバッファフォーマット
		DXGI_FORMAT DepthFormat = DXGI_FORMAT_D32_FLOAT;		// 深度バッファフォーマット
		uint32_t	FrameCount	= 2;							// フレームバッファ数（2 = ダブル、3 = トリプル）
	};

	// -------------------------------------------------------------------------------
	// コンストラクタ
	// -------------------------------------------------------------------------------
	GraphicsDevice();

	// -------------------------------------------------------------------------------
	// デストラクタ
	// -------------------------------------------------------------------------------
	~GraphicsDevice();

	// -------------------------------------------------------------------------------
	// @brief	初期化
	// 
	// @param[in]	_desc	初期化パラメータ
	// @retval	treu	初期化成功
	// @retval	false	初期化失敗
	// -------------------------------------------------------------------------------
	bool Init(const Desc& _desc);

	// -------------------------------------------------------------------------------
	// @brief	終了処理。GPU完了を待ってから全リソースを解放
	// -------------------------------------------------------------------------------
	void Term();

	// -------------------------------------------------------------------------------
	// @brief	GPUの全処理が完了するまでCPUを待機させる
	// 
	//			用途 : 
	//				- シーン切り替え時（SceneManager::ProcessSceneChange）
	//				- リソース解放前の安全確認
	//				- Term内部でも呼ばれる
	// -------------------------------------------------------------------------------
	void WaitForGPU();

	// @brief Present() 後にスワップチェインのバックバッファ番号に合わせてフレームインデックスを更新する
	void UpdateFrameIndex();

	// -------------------------------------------------------------------------------
	// ゲッター
	// -------------------------------------------------------------------------------
	ID3D12Device*		GetDevice()						const;
	ID3D12CommandQueue* GetQueue()						const;
	IDXGISwapChain3*	GetSwapChain()					const;
	CommandList*		GetCommandList()					 ;
	Fence*				GetFence()							 ;
	DescriptorPool*		GetPool(POOL_TYPE _type)		const;
	ColorTarget*		GetColorTarget(uint32_t _index)	const;
	DepthTarget*		GetDepthTarget()				const;
	uint32_t			GetFrameIndex()					const;
	uint32_t			GetFrameCount()					const;
	uint32_t			GetWidth()						const;
	uint32_t			GetHeight()						const;

private:

	// -------------------------------------------------------------------------------
	// private methods
	// -------------------------------------------------------------------------------
	bool InitDevice();
	bool InitCommandQueue();
	bool InitSwapChain();
	bool InitDescriptorPools();
	bool InitColorTargets();
	bool InitDepthTarget();
	bool InitCommandList();
	bool InitFence();

	// -------------------------------------------------------------------------------
	// private variables
	// -------------------------------------------------------------------------------
	Desc						m_Desc;
	ComPtr<ID3D12Device>		m_pDevice;
	ComPtr<ID3D12CommandQueue>	m_pQueue;
	ComPtr<IDXGISwapChain3>		m_pSwapChain;

	DescriptorPool*				m_pPool[POOL_COUNT] = {};
	std::vector<std::unique_ptr<ColorTarget>>	m_ColorTargets;
	DepthTarget					m_DepthTarget;
	CommandList					m_CommandList;
	Fence						m_Fence;

	uint32_t					m_FrameIndex	= 0;

	// コピー禁止
	GraphicsDevice	(const GraphicsDevice&) = delete;
	void operator = (const GraphicsDevice&) = delete;
};



