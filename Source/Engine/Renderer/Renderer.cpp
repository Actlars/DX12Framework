// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Renderer.h"
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
Renderer::Renderer()
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
Renderer::~Renderer() 
{ Term(); }

// -------------------------------------------------------------------------------
//		初期化
// -------------------------------------------------------------------------------
bool Renderer::Init(GraphicsDevice* _pGraphicsDevice)
{
	if (_pGraphicsDevice == nullptr)
	{
		ELOG("Renderer::Init() GraphicsDevice is nullptr");
		return false;
	}

	m_pGraphicsDevice = _pGraphicsDevice;

	// ビューポートの設定
	m_ViewPort.TopLeftX = 0.0f;
	m_ViewPort.TopLeftY = 0.0f;
	m_ViewPort.Width	= static_cast<float>(m_pGraphicsDevice->GetWidth());
	m_ViewPort.Height	= static_cast<float>(m_pGraphicsDevice->GetHeight());
	m_ViewPort.MinDepth = 0.0f;
	m_ViewPort.MaxDepth = 0.0f;

	// シザー矩形の設定
	m_Scissor.left		= 0.0f;
	m_Scissor.top		= 0.0f;
	m_Scissor.right		= static_cast<LONG>(m_pGraphicsDevice->GetWidth());
	m_Scissor.bottom	= static_cast<LONG>(m_pGraphicsDevice->GetHeight());

	return true;
}

// -------------------------------------------------------------------------------
//		終了処理
// -------------------------------------------------------------------------------
void Renderer::Term()
{
	m_pCurrentCmd		= nullptr;
	m_pGraphicsDevice	= nullptr;
}

// -------------------------------------------------------------------------------
//		フレーム開始処理
// -------------------------------------------------------------------------------
ID3D12GraphicsCommandList* Renderer::BeginFrame()
{
	if (m_pGraphicsDevice == nullptr) 
	{ return nullptr; }

	const auto frameIndex = m_pGraphicsDevice->GetFrameIndex();

	// コマンドリストのリセット
	auto* pCmd = m_pGraphicsDevice->GetCommandList()->Reset();
	if (pCmd == nullptr)
	{
		ELOG("Renderer::BeginFrame() CommandList::Reset failed");
		return nullptr;
	}
	m_pCurrentCmd = pCmd;

	// バックバッファを描画先に切り替えるバリア
	auto* pTarget = m_pGraphicsDevice->GetColorTarget(frameIndex)->GetResource();
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags					= D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource	= pTarget;
	barrier.Transition.StateBefore	= D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter	= D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	pCmd->ResourceBarrier(1, &barrier);

	// レンダーターゲットと深度バッファのクリア
	auto handleRTV = m_pGraphicsDevice->GetColorTarget(frameIndex)->GetHandleRTV()->HandleCPU;
	auto handleDSV = m_pGraphicsDevice->GetDepthTarget()->GetHandleDSV()->HandleCPU;

	pCmd->ClearRenderTargetView(handleRTV, CLEAR_COLOR, 0, nullptr);
	pCmd->ClearDepthStencilView(handleDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// レンダーターゲットの設定
	pCmd->OMSetRenderTargets(1, &handleRTV, FALSE, &handleDSV);

	// ビューポート / シザー矩形の設定
	pCmd->RSSetViewports(1, &m_ViewPort);
	pCmd->RSSetScissorRects(1, &m_Scissor);

	return pCmd;
}

// -------------------------------------------------------------------------------
//		フレーム終了処理
// -------------------------------------------------------------------------------
void Renderer::EndFrame()
{
	if (m_pCurrentCmd == nullptr || m_pGraphicsDevice == nullptr)
	{
		return;
	}

	const auto frameIndex = m_pGraphicsDevice->GetFrameIndex();

	// バックバッファを表示用に切り替えるバリア
	auto* pTarget = m_pGraphicsDevice->GetColorTarget(frameIndex)->GetResource();
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags					= D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource	= pTarget;
	barrier.Transition.StateBefore	= D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter	= D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_pCurrentCmd->ResourceBarrier(1, &barrier);

	// コマンドリストをクローズしてGPUに投入
	m_pCurrentCmd->Close();

	ID3D12CommandList* ppLists[] = { m_pCurrentCmd };
	m_pGraphicsDevice->GetQueue()->ExecuteCommandLists(1, ppLists);

	m_pCurrentCmd = nullptr;
}

// -------------------------------------------------------------------------------
//		画面表示
// -------------------------------------------------------------------------------
void Renderer::Present(uint32_t _syncInterval)
{
	if (m_pGraphicsDevice == nullptr) 
	{ return; }

	// バックバッファを画面に表示
	m_pGraphicsDevice->GetSwapChain()->Present(_syncInterval, 0);

	// GPU完了を待つ
	// 次フレームで同じバッファに書き込む前にGPUが使い終わるのを確認する
	m_pGraphicsDevice->WaitForGPU();

	// フレームインデックスを更新する
	// スワップチェインが管理するバックバッファ番号に合わせる
	// ※ m_FrameIndex は GraphicsDevice 側で管理するため
	//   ここでは直接変更できない。GraphicsDevice に UpdateFrameIndex() を追加するか、
	//   Present 後に GetCurrentBackBufferIndex() を呼ぶ必要がある。
	// 現状は GraphicsDevice::Present() を分離したので GraphicsDevice 側で管理する。
	m_pGraphicsDevice->UpdateFrameIndex();
}

// -------------------------------------------------------------------------------
//		フレームインデックスの取得
// -------------------------------------------------------------------------------
uint32_t Renderer::GetFrameIndex() const
{
	if (m_pGraphicsDevice == nullptr) 
	{ return 0; }

	return m_pGraphicsDevice->GetFrameIndex();
}