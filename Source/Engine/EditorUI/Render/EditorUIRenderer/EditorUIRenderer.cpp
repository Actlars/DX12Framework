// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "EditorUIRenderer.h"

#include <Engine/RHI/Core/Device/Device.h>
#include <Engine/RHI/Resource/Texture/Texture.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

namespace
{
	// EditorUI::UIVertexのメモリレイアウトと対応する頂点レイアウト
	const D3D12_INPUT_ELEMENT_DESC KUIInputElements[] =
	{
		{ "POSITION",	0, DXGI_FORMAT_R32G32_FLOAT,	0, offsetof(EditorUI::UIVertex, Position),	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,	0, offsetof(EditorUI::UIVertex, UV),		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "COLOR",		0, DXGI_FORMAT_R8G8B8A8_UNORM,	0, offsetof(EditorUI::UIVertex, Color),		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,	0},
	};
}

bool EditorUIRenderer::Init(RHI::Device* _pDevice, uint32_t _frameInFlightCount)
{
	if (_pDevice == nullptr) 
	{ return false; }

	m_pDevice		= _pDevice;
	auto* pDevice	= _pDevice->GetDevice();

	// -------------------------------------------------------------------------------
	// RootSignature / PSO読み込み
	// -------------------------------------------------------------------------------
	if (!m_RootSignatureLayout.LoadFromJson(pDevice, L"Assets/Config/Json/RootSignature/EditorUI/EditorUI.json"))
	{
		ELOG("EditorUIRenderer::Init() RootSignature load failed");
		return false;
	}

	auto* pRootSignature = m_RootSignatureLayout.GetRootSignature();
	D3D12_INPUT_LAYOUT_DESC inputLayout{ KUIInputElements, _countof(KUIInputElements) };
	m_pPSO = _pDevice->GetPipelineCache()->GetOrCreate(
		pDevice, L"Assets/Config/Json/PipelineState/EditorUI/EditorUI.json",
		pRootSignature, inputLayout);

	if (m_pPSO == nullptr)
	{
		ELOG("EditorUIRenderer::Init() PSO creation failed");
		return false;
	}

	// -------------------------------------------------------------------------------
	// フレームインフライト数分のジオメトリバッファを用意する
	// -------------------------------------------------------------------------------
	m_FrameGeometries.clear();
	m_FrameGeometries.reserve(_frameInFlightCount);
	for (uint32_t i = 0; i < _frameInFlightCount; ++i)
	{
		m_FrameGeometries.push_back(std::make_unique<UIGeometry>());
	}

	// -------------------------------------------------------------------------------
	// 白テクスチャの登録、既存のダミーテクスチャを流用
	// -------------------------------------------------------------------------------
	auto* pDummy = _pDevice->GetDummyTexture();
	if (pDummy == nullptr)
	{
		ELOG("EditorUIRenderer::Init() GetDummyTexture failed");
		return false;
	}

	m_WhiteTexture = RegisterTexture(pDummy);

	return true;
}

void EditorUIRenderer::Term()
{
	m_TextureHandles.clear();
	m_FrameGeometries.clear();
	m_pDevice = nullptr;
}

void EditorUIRenderer::Render(
	const EditorUI::Context::CompositedFrame&	_compositedFrame, 
	ID3D12GraphicsCommandList*					_pCmd, 
	uint32_t									_frameIndex, 
	float										_screenWidth, 
	float										_screenHeight)
{
	if (_pCmd == nullptr || _frameIndex >= m_FrameGeometries.size())
	{
		return;
	}

	UIGeometry& geometry = *m_FrameGeometries[_frameIndex];
	const std::vector<UIFlatDrawCommand>& commands =
		geometry.Upload(_compositedFrame.WindowDrawLists, m_pDevice->GetDevice());

	// 描画するものがない
	if (commands.empty())
	{ return; }

	// -------------------------------------------------------------------------------
	// バッファに直接重ね書きし、深度は使わない
	// -------------------------------------------------------------------------------
	auto* pRTV = m_pDevice->GetColorTarget(_frameIndex)->GetHandleRTV();
	_pCmd->OMSetRenderTargets(1, &pRTV->HandleCPU, FALSE, nullptr);

	// -------------------------------------------------------------------------------
	// パイプライン設定
	// -------------------------------------------------------------------------------
	_pCmd->SetGraphicsRootSignature(m_RootSignatureLayout.GetRootSignature());
	_pCmd->SetPipelineState(m_pPSO);
	_pCmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_VERTEX_BUFFER_VIEW	vbv = geometry.GetVertexBufferView();
	D3D12_INDEX_BUFFER_VIEW		ibv = geometry.GetIndexBufferView();
	_pCmd->IASetVertexBuffers(0, 1, &vbv);
	_pCmd->IASetIndexBuffer(&ibv);

	// -------------------------------------------------------------------------------
	// ディスクリプタヒープのバインド
	// -------------------------------------------------------------------------------
	ID3D12DescriptorHeap* heaps[] = { m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES)->GetHeap() };
	_pCmd->SetDescriptorHeaps(_countof(heaps), heaps);

	// -------------------------------------------------------------------------------
	// ビューポート（画面全体）
	// -------------------------------------------------------------------------------
	D3D12_VIEWPORT viewport{ 0.0f,0.0f,_screenWidth,_screenHeight, 0.0f,1.0f };
	_pCmd->RSSetViewports(1, &viewport);

	// -------------------------------------------------------------------------------
	// ルート定数は画面サイズが変わらない限り毎コマンド同じなのでループの外で1回だけ積む
	// -------------------------------------------------------------------------------
	struct { float InvW, InvH; }screenParams{ 1.0f / _screenWidth,1.0f / _screenHeight };
	_pCmd->SetGraphicsRoot32BitConstants(m_RootSignatureLayout.GetSlot("ScreenParams"), 2, &screenParams, 0);

	// -------------------------------------------------------------------------------
	// マージ済みコマンドを順番に処理
	// -------------------------------------------------------------------------------
	const uint32_t mainTextureSlot = m_RootSignatureLayout.GetSlot("MainTexture");

	for (const UIFlatDrawCommand& cmd : commands)
	{
		D3D12_RECT scissor
		{
			static_cast<LONG>(cmd.ClipRect.Min.x), static_cast<LONG>(cmd.ClipRect.Min.y),
			static_cast<LONG>(cmd.ClipRect.Max.x), static_cast<LONG>(cmd.ClipRect.Max.y)
		};
		_pCmd->RSSetScissorRects(1, &scissor);

		auto it = m_TextureHandles.find(cmd.Texture);
		D3D12_GPU_DESCRIPTOR_HANDLE handle = (it != m_TextureHandles.end())
			? it->second
			: m_TextureHandles[m_WhiteTexture];	// 見つからない場合は白テクスチャ

		_pCmd->SetGraphicsRootDescriptorTable(mainTextureSlot, handle);
		_pCmd->DrawIndexedInstanced(cmd.ElementCount, 1, cmd.IndexOffset, 0, 0);
	}

}

EditorUI::TextureId EditorUIRenderer::RegisterTexture(const RHI::Texture* _pTexture)
{
	if (_pTexture == nullptr)
	{ return 0; }
	
	// Texture::GetIndex()はDescriptorPool内のスロット番号で
	// 同じテクスチャなら常に同じ値を返す。これをそのままTextureIdとして使いまわす
	EditorUI::TextureId id = static_cast<EditorUI::TextureId>(_pTexture->GetIndex());
	m_TextureHandles[id] = _pTexture->GetHandleGPU();
	return id;
}

void EditorUIRenderer::UnregisterTexture(EditorUI::TextureId _id)
{
	m_TextureHandles.erase(_id);
}
