#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------

struct PipelineStateDesc
{
	std::wstring			VSPath;
	std::wstring			PSPath;
	D3D12_CULL_MODE			CullMode	= D3D12_CULL_MODE_BACK;
	D3D12_COMPARISON_FUNC	DepthFunc	= D3D12_COMPARISON_FUNC_LESS_EQUAL;
	bool					DepthEnable = true;
	bool					BlendEnable = false;
	DXGI_FORMAT				RTVFormat	= DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT				DSVFormat	= DXGI_FORMAT_D32_FLOAT;
};
