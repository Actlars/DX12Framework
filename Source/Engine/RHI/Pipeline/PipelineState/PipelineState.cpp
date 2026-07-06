// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "PipelineState.h"
#include <Engine/Utility/Debug/Logger/Logger.h>
#include <Engine/Utility/FileUtil/FileUtil.h>
#include <Engine/Utility/StringUtil/StringUtil.h>

namespace
{
	D3D12_CULL_MODE ToCullMode(const std::string& _s)
	{
		if (_s == "NONE")	{ return D3D12_CULL_MODE_NONE;	}
		if (_s == "FRONT")	{ return D3D12_CULL_MODE_FRONT; }
		if (_s == "BACK")	{ return D3D12_CULL_MODE_BACK;	}
		ELOG("PipelineState::ToCullMode() unknown value : %s", _s.c_str());
		return D3D12_CULL_MODE_BACK;
	}

	D3D12_FILL_MODE ToFillMode(const std::string& _s)
	{
		if (_s == "WIREFRAME")	{ return D3D12_FILL_MODE_WIREFRAME; }
		if (_s == "SOLID")		{ return D3D12_FILL_MODE_SOLID; }
		ELOG("PipelineState::ToFillMode() unknown value : %s", _s.c_str());
		return D3D12_FILL_MODE_SOLID;
	}

	D3D12_COMPARISON_FUNC ToComparisonFunc(const std::string& _s)
	{
		if (_s == "NEVER")			{ return D3D12_COMPARISON_FUNC_NEVER; }
		if (_s == "LESS")			{ return D3D12_COMPARISON_FUNC_LESS; }
		if (_s == "EQUAL")			{ return D3D12_COMPARISON_FUNC_EQUAL; }
		if (_s == "LESS_EQUAL")		{ return D3D12_COMPARISON_FUNC_LESS_EQUAL; }
		if (_s == "GREATER")		{ return D3D12_COMPARISON_FUNC_GREATER; }
		if (_s == "NOT_EQUAL")		{ return D3D12_COMPARISON_FUNC_NOT_EQUAL; }
		if (_s == "GREATER_EQUAL")	{ return D3D12_COMPARISON_FUNC_GREATER_EQUAL; }
		if (_s == "ALWAYS")			{ return D3D12_COMPARISON_FUNC_ALWAYS; }
		ELOG("PipelineState::ToComparisonFunc : unknown value : %s", _s.c_str());
		return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	}

	D3D12_DEPTH_WRITE_MASK ToDepthWriteMask(const std::string& _s)
	{
		if (_s == "ZERO")	{ return D3D12_DEPTH_WRITE_MASK_ZERO; }
		if (_s == "ALL")	{ return D3D12_DEPTH_WRITE_MASK_ALL; }
		ELOG("PipelineState::ToComparisonFunc : unknown value : %s", _s.c_str());
		return D3D12_DEPTH_WRITE_MASK_ALL;
	}

	D3D12_PRIMITIVE_TOPOLOGY_TYPE ToTopologyType(const std::string& _s)
	{
		if (_s == "POINT")		{ return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; }
		if (_s == "LINE")		{ return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; }
		if (_s == "TRIANGLE")	{ return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; }
		ELOG("PipelineState::ToTopologyType : unknown value : %s", _s.c_str());
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	}

	DXGI_FORMAT ToFormat(const std::string& _s)
	{
		if (_s == "R8G8B8A8_UNORM") { return DXGI_FORMAT_R8G8B8A8_UNORM; }
		if (_s == "R16G16B16A16_FLOAT") { return DXGI_FORMAT_R16G16B16A16_FLOAT; }
		if (_s == "D32_FLOAT") { return DXGI_FORMAT_D32_FLOAT; }
		if (_s == "UNKNOWN") { return DXGI_FORMAT_UNKNOWN; }
		ELOG("PipelineState::ToFormat : unknown value : %s", _s.c_str());
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	}
}

bool RHI::PipelineState::LoadFromJson(
	ID3D12Device*					_pDevice, 
	const std::wstring&				_jsonPath, 
	ID3D12RootSignature*			_pRootSignature, 
	const D3D12_INPUT_LAYOUT_DESC&	_inputLayout)
{
	if (_pDevice == nullptr || _pRootSignature == nullptr)
	{ return false; }

	std::ifstream ifs(_jsonPath);
	if (!ifs.is_open())
	{
		ELOG("PipelineState::LoadFromJson() : failed to open %ls", _jsonPath.c_str());
		return false;
	}

	try
	{
		nlohmann::json json;
		ifs >> json;

		// シェーダーの読み込み
		std::wstring vsPath, psPath;
		std::wstring vsName = StringUtil::ToWString(json.at("VS").get<std::string>());
		std::wstring psName = StringUtil::ToWString(json.at("PS").get<std::string>());

		if (!SearchFilePath(vsName.c_str(), vsPath))
		{
			ELOG("PipelineState::FromToJson() : VS not found : %ls", vsName.c_str());
			return false; 
		}
		if (!SearchFilePath(psName.c_str(), psPath))
		{
			ELOG("PipelineState::FromToJSon() : VS not found : %ls", psName.c_str());
			return false;
		}

		ComPtr<ID3DBlob> pVS, pPS;
		if (FAILED(D3DReadFileToBlob(vsPath.c_str(), pVS.GetAddressOf()))) { return false; }
		if (FAILED(D3DReadFileToBlob(psPath.c_str(), pPS.GetAddressOf()))) { return false; }

		// ラスタライザ設定
		D3D12_RASTERIZER_DESC rsDesc = {};
		rsDesc.FillMode					= ToFillMode(json.value("FillMode", std::string("SOLID")));
		rsDesc.CullMode					= ToCullMode(json.value("CullMode", std::string("BACK")));
		rsDesc.FrontCounterClockwise	= FALSE;
		rsDesc.DepthBias				= D3D12_DEFAULT_DEPTH_BIAS;
		rsDesc.DepthBiasClamp			= D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rsDesc.SlopeScaledDepthBias		= D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rsDesc.DepthClipEnable			= TRUE;

		// ブレンド設定
		D3D12_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable		= FALSE;
		blendDesc.IndependentBlendEnable	= FALSE;
		const bool blendEnable = json.value("blendEnable", false);
		for (auto& rt : blendDesc.RenderTarget)
		{
			rt.BlendEnable				= blendEnable ? TRUE : FALSE;
			rt.SrcBlend					= blendEnable ? D3D12_BLEND_SRC_ALPHA : D3D12_BLEND_ONE;
			rt.DestBlend				= blendEnable ? D3D12_BLEND_INV_SRC_ALPHA : D3D12_BLEND_ZERO;
			rt.BlendOp					= D3D12_BLEND_OP_ADD;
			rt.SrcBlendAlpha			= D3D12_BLEND_ONE;
			rt.DestBlendAlpha			= D3D12_BLEND_ZERO;
			rt.BlendOpAlpha				= D3D12_BLEND_OP_ADD;
			rt.LogicOp					= D3D12_LOGIC_OP_NOOP;
			rt.RenderTargetWriteMask	= D3D12_COLOR_WRITE_ENABLE_ALL;
		}

		// 深度ステンシル設定
		D3D12_DEPTH_STENCIL_DESC dssDesc = {};
		dssDesc.DepthEnable		= json.value("DepthEnable", true) ? TRUE : FALSE;
		dssDesc.DepthWriteMask	= ToDepthWriteMask(json.value("DepthWriteMask", std::string("ALL")));
		dssDesc.DepthFunc		= ToComparisonFunc(json.value("DepthFunc", std::string("LESS_EQUAL")));
		dssDesc.StencilEnable	= FALSE;

		// PSOの組み立て
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout				= _inputLayout;
		psoDesc.pRootSignature			= _pRootSignature;
		psoDesc.VS						= { pVS->GetBufferPointer(), pVS->GetBufferSize() };
		psoDesc.PS						= { pPS->GetBufferPointer(), pPS->GetBufferSize() };
		psoDesc.RasterizerState			= rsDesc;
		psoDesc.BlendState				= blendDesc;
		psoDesc.DepthStencilState		= dssDesc;
		psoDesc.SampleMask				= UINT_MAX;
		psoDesc.PrimitiveTopologyType	= ToTopologyType(json.value("PrimitiveTopologyType", std::string("TRIANGLE")));
		psoDesc.NumRenderTargets		= 1;
		psoDesc.RTVFormats[0]			= ToFormat(json.value("RTVFormat", std::string("R8G8BB8A8_UNORM")));
		psoDesc.DSVFormat				= ToFormat(json.value("DSVFormat", std::string("D32_FLOAT")));
		psoDesc.SampleDesc.Count		= 1;

		auto hr = _pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS((m_pPSO.GetAddressOf())));
		if (FAILED(hr))
		{
			ELOG("PipelineState::LoadFromJson() : CreateGraphicsPipelineState failed hr = 0x%08X", hr);
			return false;
		}
	}
	catch (const std::exception& e)
	{
		ELOG("PipelineState::LoadFromJson() : exeption : %s", e.what());
		return false;
	}

	return true;
}
