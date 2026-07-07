#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/Renderer/PostProcess/IPostProcessEffect.h>
#include <Engine/RHI/Pipeline/RootSignature/RootSignatureLayout/RootSignatureLayout.h>
#include <Engine/RHI/Resource/Buffer/ConstantBuffer/ConstantBuffer.h>

class BloomEffect : public IPostProcessEffect
{
public:

	bool Init(RHI::Device* _pDevice)override;
	void AddPasses(RG::RenderGraph& _graph, RG::Handle& _sceneColorHandle, RG::Handle _backBufferHandle, bool _isLast)override;

	void SetFullViewport(ID3D12GraphicsCommandList* _pCmd, uint32_t _width, uint32_t _height);

private:

	struct BlurParamsCB
	{
		DirectX::XMFLOAT2 TexelSize;
		DirectX::XMFLOAT2 Padding;
	};

	RHI::Device*							m_pDevice = nullptr;
	RHI::RootSignatureLayout				m_RootSignatureLayout;
	std::unique_ptr<RHI::ConstantBuffer>	m_BlurParams;

	ID3D12PipelineState* m_pExtractPSO		= nullptr;
	ID3D12PipelineState* m_pBlurPSO			= nullptr;
	ID3D12PipelineState* m_pCompositePSO	= nullptr;

};
