#pragma once

// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "../Framework/Texture/TextureManager/TextureManager.h"
#include "../Framework/Pool/DescriptorPool/DescriptorPool.h"
#include "../Framework/Mesh/Mesh.h"

// -------------------------------------------------------------------------------
// Linker
// -------------------------------------------------------------------------------

// -------------------------------------------------------------------------------
// namespace
// -------------------------------------------------------------------------------
using Microsoft::WRL::ComPtr;

/**
* Application Class
*/
class Application
{
	// -------------------------------------------------------------------------------
	// list of friend classes and methods
	// -------------------------------------------------------------------------------
	/* NOTHING */

public:

	// -------------------------------------------------------------------------------
	// public variables
	// -------------------------------------------------------------------------------
	/* NOTHING */
	struct alignas(256) Transform
	{
		DirectX::XMMATRIX World;	// ワールド行列
		DirectX::XMMATRIX View;		// ビュー行列
		DirectX::XMMATRIX Proj;		// 射影行列
	};

	// -------------------------------------------------------------------------------
	// ConstantBufferViewstructure
	// -------------------------------------------------------------------------------
	template<typename T>
	struct ConstantBufferView
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC	Desc;			// 定数バッファの構成設定
		D3D12_CPU_DESCRIPTOR_HANDLE			HandleCPU;	// CPUディスクリプタハンドル
		D3D12_GPU_DESCRIPTOR_HANDLE			HandleGPU;	// GPUディスクリプタハンドル
		T* pBuffer;										// バッファ先頭へのポインタ
	};

	// -------------------------------------------------------------------------------
	// public methods
	// -------------------------------------------------------------------------------
	Application(uint32_t width, uint32_t height);
	~Application();
	void Run();

private:

	// -------------------------------------------------------------------------------
	// private variables
	// -------------------------------------------------------------------------------
	static const uint32_t FrameCount = 2;			// フレームバッファ数

	HINSTANCE	m_hInst;	// インスタンスハンドル
	HWND		m_hWnd;		// ウィンドウハンドル
	uint32_t	m_Width;	// ウィンドウの横幅
	uint32_t	m_Height;	// ウィンドウの縦幅

	ComPtr<ID3D12Device>				m_pDevice;						// デバイス
	ComPtr<ID3D12CommandQueue>			m_pQueue;						// コマンドキュー
	ComPtr<IDXGISwapChain3>				m_pSwapChain;					// スワップチェイン
	ComPtr<ID3D12Resource>				m_pColorBuffer[FrameCount];		// カラーバッファ
	ComPtr<ID3D12CommandAllocator>		m_pCmdAllocator[FrameCount];	// コマンドアロケータ
	ComPtr<ID3D12GraphicsCommandList>	m_pCmdList;						// コマンドリスト
	ComPtr<ID3D12DescriptorHeap>		m_pHeapRTV;						// ディスクリプタヒープ（レンダーターゲットビュー）
	ComPtr<ID3D12DescriptorHeap>		m_pHeapDSV;						// ディスクリプタヒープ（深度バッファビュー）
	ComPtr<ID3D12Fence>					m_pFence;						// フェンス
	ComPtr<ID3D12DescriptorHeap>		m_pHeapCBV;						// ディスクリプタヒープ（定数バッファー・シェーダーリソースビュー・アンオーダーアクセスビュー）
	ComPtr<ID3D12Resource>				m_pVB;							// 頂点バッファ
	ComPtr<ID3D12Resource> 				m_pIB;							// インデックスバッファ
	ComPtr<ID3D12Resource>				m_pCB[FrameCount * 2];			// 定数バッファ
	ComPtr<ID3D12Resource>				m_pDepthBuffer;					// 深度バッファ
	ComPtr<ID3D12RootSignature>			m_pRootSignature;				// ルートシグニチャ
	ComPtr<ID3D12PipelineState>			m_pPSO;							// パイプラインステート

	HANDLE								m_FenceEvent;					// フェンスイベント
	uint64_t							m_FenceCounter[FrameCount];		// フェンスカウンター
	uint32_t							m_FrameIndex;					// フレーム番号
	D3D12_CPU_DESCRIPTOR_HANDLE			m_HandleRTV[FrameCount];		// CPUディスクリプタ（レンダーターゲット）
	D3D12_CPU_DESCRIPTOR_HANDLE         m_HandleDSV;					// CPUディスクリプタ（深度ステンシル）	
	D3D12_VERTEX_BUFFER_VIEW			m_VBV;							// 頂点バッファービュー
	D3D12_INDEX_BUFFER_VIEW				m_IBV;							// インデックスバッファービュー
	D3D12_VIEWPORT						m_Viewport;						// ビューポート
	D3D12_RECT							m_Scissor;						// シザー矩形
	ConstantBufferView<Transform>		m_CBV[FrameCount * 2];			// 定数バッファービュー
	float								m_RotateAngle = 0;				// 回転角
	TextureManager						m_TextureManager;				// テクスチャマネージャ
	std::vector<Mesh>					m_Meshes;						// メッシュ
	std::vector<Material>				m_Materials;					// マテリアル
	uint32_t							m_TexSlot = 0;					// ロードしたテクスチャのスロット番号

	struct Vertex
	{
		DirectX::XMFLOAT3 Position;	// 位置情報
		DirectX::XMFLOAT2 UV;		// UV情報
		DirectX::XMFLOAT4 Color;	// 頂点カラー
	};

	// -------------------------------------------------------------------------------
	// private methods
	// -------------------------------------------------------------------------------
	bool InitApplication();
	void TermApplication();
	bool InitWnd();
	void TermWnd();
	void MainLoop();
	bool InitD3D();
	void TermD3D();
	void Render();
	void WaitGpu();
	void Present(uint32_t interval);
	bool OnInit();

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

};
