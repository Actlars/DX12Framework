#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/RHI/Pipeline/RootSignature/RootSignatureLayout/RootSignatureLayout.h>
#include <Engine/EditorUI/Core/Context/Context.h>
#include <Engine/EditorUI/Core/Context/FrameContext/FrameOutput.h>
#include <Engine/EditorUI/Render/UIFrameGeometry/UIFrameGeometry.h>

namespace RHI { class Device; class Texture; }

// -------------------------------------------------------------------------------
// EditorUIRenderer class
// 
// 概要 : 
//	EditorUIの描画コマンドを、渡されたコマンドリストに積むクラス
// -------------------------------------------------------------------------------
class EditorUIRenderer
{
public :
	
	bool Init(RHI::Device* _pDevice, uint32_t _frameInFlightCount);

	void Term();

	// CompositedFrameを受け取り、実際にコマンドリストに積む
	void Render(
		const EditorUI::FrameOutput&	_compositedFrame,
		ID3D12GraphicsCommandList*		_pCmd,
		uint32_t						_frameIndex,
		float							_screenWidth,
		float							_screenHeight);

	bool MakeValidScissorRect(
		const EditorUI::Rect2D& _clipRect,
		float					_frameBufferWidth,
		float					_frameBufferHeight,
		D3D12_RECT&				_outScissor);


	// テクスチャをEditorUIから使えるように登録し、TextureIdを返す
	EditorUI::TextureId RegisterTexture(const RHI::Texture* _pTexture);
	void UnregisterTexture(EditorUI::TextureId _id);

private:

	RHI::Device*				m_pDevice	= nullptr;
	RHI::RootSignatureLayout	m_RootSignatureLayout;
	ID3D12PipelineState*		m_pPSO		= nullptr;

	std::vector<std::unique_ptr<UIGeometry>> m_FrameGeometries;	// frameInFlightCount個

	// TextureId → 実際のGPUハンドルの逆引き表
	std::unordered_map<EditorUI::TextureId, D3D12_GPU_DESCRIPTOR_HANDLE> m_TextureHandles;
	EditorUI::TextureId m_WhiteTexture = 0;

};
