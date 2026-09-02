// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Renderer.h"
#include <Engine/Utility/Debug/Logger/Logger.h>
#include <Engine/RHI/Resource/ResourceStateTracker/ResourceStateTracker.h>

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
bool Renderer::Init(RHI::Device* _pGraphicsDevice)
{
	if (_pGraphicsDevice == nullptr)
	{
		ELOG("Renderer::Init() GraphicsDevice is nullptr");
		return false;
	}

	m_pDevice = _pGraphicsDevice;

	// ビューポートの設定
	m_ViewPort.TopLeftX = 0.0f;
	m_ViewPort.TopLeftY = 0.0f;
	m_ViewPort.Width	= static_cast<float>(m_pDevice->GetWidth());
	m_ViewPort.Height	= static_cast<float>(m_pDevice->GetHeight());
	m_ViewPort.MinDepth = 0.0f;
	m_ViewPort.MaxDepth = 1.0f;

	// シザー矩形の設定
	// D3D12_RECTの各成分はLONGのため、0.0fではなく整数で入れる
	m_Scissor.left		= 0;
	m_Scissor.top		= 0;
	m_Scissor.right		= static_cast<LONG>(m_pDevice->GetWidth());
	m_Scissor.bottom	= static_cast<LONG>(m_pDevice->GetHeight());

	return true;
}

// -------------------------------------------------------------------------------
//		終了処理
// -------------------------------------------------------------------------------
void Renderer::Term()
{
	m_pDevice	= nullptr;
}

// -------------------------------------------------------------------------------
//		フレーム開始処理
// -------------------------------------------------------------------------------
ID3D12GraphicsCommandList* Renderer::BeginFrame()
{
	if (m_pDevice == nullptr) 
	{ return nullptr; }

	// 現在のフレームインデックスを取得
	const auto frameIndex = m_pDevice->GetFrameIndex();

	// コマンドリストのリセット（Fenceを渡して、必要な時だけ待機させる）
	auto* pCmd = m_pDevice->GetCommandList()->Reset(m_pDevice->GetFence());
	if (pCmd == nullptr)
	{
		ELOG("Renderer::BeginFrame() CommandList::Reset failed");
		return nullptr;
	}

	//// バックバッファを描画先に切り替えるバリア
	//auto* pTarget = m_pDevice->GetColorTarget(frameIndex)->GetResource();
	//D3D12_RESOURCE_BARRIER barrier = {};
	//barrier.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//barrier.Flags					= D3D12_RESOURCE_BARRIER_FLAG_NONE;
	//barrier.Transition.pResource	= pTarget;
	//barrier.Transition.StateBefore	= D3D12_RESOURCE_STATE_PRESENT;
	//barrier.Transition.StateAfter	= D3D12_RESOURCE_STATE_RENDER_TARGET;
	//barrier.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	//pCmd->ResourceBarrier(1, &barrier);

	auto* pTarget = m_pDevice->GetColorTarget(frameIndex)->GetResource();
	// 手動バリア構築の代わりにトラッカーへ移譲
	auto* pTracker = m_pDevice->GetResourceStateTracker();
	pTracker->TransitionResource(pTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
	pTracker->FlushBarriers(pCmd);

	// レンダーターゲットと深度バッファのクリア
	auto handleRTV = m_pDevice->GetColorTarget(frameIndex)->GetHandleRTV()->HandleCPU;
	auto handleDSV = m_pDevice->GetDepthTarget()->GetHandleDSV()->HandleCPU;

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
void Renderer::EndFrame(ID3D12GraphicsCommandList* _pCmd)
{
	if (_pCmd == nullptr || m_pDevice == nullptr)
	{
		return;
	}

	const auto frameIndex = m_pDevice->GetFrameIndex();

	// バックバッファを表示用に切り替えるバリア
	auto* pTarget = m_pDevice->GetColorTarget(frameIndex)->GetResource();
	/*D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags					= D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource	= pTarget;
	barrier.Transition.StateBefore	= D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter	= D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	_pCmd->ResourceBarrier(1, &barrier);*/

	auto* pTracker = m_pDevice->GetResourceStateTracker();
	pTracker->TransitionResource(pTarget, D3D12_RESOURCE_STATE_PRESENT);
	pTracker->FlushBarriers(_pCmd);

	// コマンドリストをクローズしてGPUに投入
	// Closeに失敗した場合、コマンドリストは記録中のまま残り、
	// 次フレーム以降のResetがすべて失敗し続ける
	// 原因の切り分けができるよう、ここで必ず結果を確認する
	const HRESULT closeResult = _pCmd->Close();
	if (FAILED(closeResult))
	{
		ELOG("Renderer::EndFrame() CommandList::Close failed (hr = 0x%08X)", static_cast<unsigned int>(closeResult));

		// 何が不正だったかはデバッグレイヤーだけが知っているので、ここで吐き出す
		m_pDevice->FlushDebugMessages();
		return;
	}

	ID3D12CommandList* ppLists[] = { _pCmd };
	m_pDevice->GetQueue()->ExecuteCommandLists(1, ppLists);

	// 実行直後にSignalを発行して（待たない）
	// 今使ったアロケータの完了フェンス値として記録する
	auto value = m_pDevice->GetFence()->Signal(m_pDevice->GetQueue());
	m_pDevice->GetCommandList()->RecordFenceValue(value);

	_pCmd = nullptr;
}

// -------------------------------------------------------------------------------
//		画面表示
// -------------------------------------------------------------------------------
void Renderer::Present(uint32_t _syncInterval)
{
	if (m_pDevice == nullptr) 
	{ return; }

	// バックバッファを画面に表示
	m_pDevice->GetSwapChain()->Present(_syncInterval, 0);

	// フレームインデックスを更新する
	// スワップチェインが管理するバックバッファ番号に合わせる
	// ※ m_FrameIndex は GraphicsDevice 側で管理するため
	//   ここでは直接変更できない。GraphicsDevice に UpdateFrameIndex() を追加するか、
	//   Present 後に GetCurrentBackBufferIndex() を呼ぶ必要がある。
	// 現状は GraphicsDevice::Present() を分離したので GraphicsDevice 側で管理する。
	m_pDevice->UpdateFrameIndex();
	m_pDevice->GetTransientResourcePool()->ReleaseAll();
}

// -------------------------------------------------------------------------------
//		フレームインデックスの取得
// -------------------------------------------------------------------------------
uint32_t Renderer::GetFrameIndex() const
{
	if (m_pDevice == nullptr) 
	{ return 0; }

	return m_pDevice->GetFrameIndex();
}
