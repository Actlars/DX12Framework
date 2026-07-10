// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "PipelineState.h"
#include <Engine/RHI/Pipeline/PipelineState/PipelineStream/PipelineStream.h>
#include <Engine/Utility/Debug/Logger/Logger.h>
#include <Engine/Utility/FileUtil/FileUtil.h>
#include <Engine/Utility/StringUtil/StringUtil.h>
#include <Engine/Utility/JsonLoader/JsonLoader.h>

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

	// Jsonロード
	nlohmann::json json;
	if (!JsonLoader::Load(_jsonPath, json))
	{
		ELOG("PipelineState::LoadFromJson() : JsonLoader::Load failed path = %ls", _jsonPath.c_str());
		return false;
	}

	// "MS"キーの有無でMeshShaderパイプラインかどうか判定
	if (json.contains("MS"))
	{
		return LoadFromJsonMeshShader(_pDevice, json, _pRootSignature);
	}
	else
	{
		return LoadFromJsonGraphics(_pDevice, json, _pRootSignature, _inputLayout);
	}
}

// -------------------------------------------------------------------------------
// @brief	VS + PS 用のPSO生成
// -------------------------------------------------------------------------------
bool RHI::PipelineState::LoadFromJsonGraphics(
	ID3D12Device*					_pDevice, 
	const nlohmann::json&			_json,
	ID3D12RootSignature*			_pRootSignature,
	const D3D12_INPUT_LAYOUT_DESC&	_inputLayout)
{
	try
	{
		// シェーダーの読み込み
		std::wstring vsPath, psPath;
		std::wstring vsName = StringUtil::ToWString(_json.at("VS").get<std::string>());
		std::wstring psName = StringUtil::ToWString(_json.at("PS").get<std::string>());

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
		rsDesc.FillMode					= ToFillMode(_json.value("FillMode", std::string("SOLID")));
		rsDesc.CullMode					= ToCullMode(_json.value("CullMode", std::string("BACK")));
		rsDesc.FrontCounterClockwise	= FALSE;
		rsDesc.DepthBias				= D3D12_DEFAULT_DEPTH_BIAS;
		rsDesc.DepthBiasClamp			= D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rsDesc.SlopeScaledDepthBias		= D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rsDesc.DepthClipEnable			= TRUE;

		// ブレンド設定
		D3D12_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable		= FALSE;
		blendDesc.IndependentBlendEnable	= FALSE;
		const bool blendEnable				= _json.value("blendEnable", false);
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
		dssDesc.DepthEnable		= _json.value("DepthEnable", true) ? TRUE : FALSE;
		dssDesc.DepthWriteMask	= ToDepthWriteMask(_json.value("DepthWriteMask", std::string("ALL")));
		dssDesc.DepthFunc		= ToComparisonFunc(_json.value("DepthFunc", std::string("LESS_EQUAL")));
		dssDesc.StencilEnable	= FALSE;

		// PSOの組み立て
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = _inputLayout;
		psoDesc.pRootSignature = _pRootSignature;
		psoDesc.VS = { pVS->GetBufferPointer(), pVS->GetBufferSize() };
		psoDesc.PS = { pPS->GetBufferPointer(), pPS->GetBufferSize() };
		psoDesc.RasterizerState = rsDesc;
		psoDesc.BlendState = blendDesc;
		psoDesc.DepthStencilState = dssDesc;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = ToTopologyType(_json.value("PrimitiveTopologyType", std::string("TRIANGLE")));
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = ToFormat(_json.value("RTVFormat", std::string("R8G8B8A8_UNORM")));
		psoDesc.DSVFormat = ToFormat(_json.value("DSVFormat", std::string("D32_FLOAT")));
		psoDesc.SampleDesc.Count = 1;

		auto hr = _pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS((m_pPSO.GetAddressOf())));
		if (FAILED(hr))
		{
			ELOG("PipelineState::LoadFromJsonGraphics() : CreateGraphicsPipelineState failed hr = 0x%08X", hr);
			return false;
		}
	}
	catch (const std::exception& e)
	{
		ELOG("PipelineState::LoadFromJsonGraphics() : exception : %s", e.what());
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------
// @brief	MeshShaderパイプライン用のPSO生成
//			PipelineStateStream経由で生成
// -------------------------------------------------------------------------------
bool RHI::PipelineState::LoadFromJsonMeshShader(
	ID3D12Device* _pDevice, 
	const nlohmann::json& _json,
	ID3D12RootSignature* _pRootSignature)
{
	try
	{
		std::wstring msPath;
		std::wstring msName = StringUtil::ToWString(_json.at("MS").get<std::string>());
		if (!SearchFilePath(msName.c_str(), msPath))
		{
			ELOG("PipelineState::LoadFromJsonMeshShader() : MS not found %ls", msName.c_str());
			return false;
		}
		ComPtr<ID3DBlob> pMS;
		if (FAILED(D3DReadFileToBlob(msPath.c_str(), pMS.GetAddressOf()))) 
		{ return false; }

		std::wstring psPath;
		std::wstring psName = StringUtil::ToWString(_json.at("PS").get<std::string>());
		if (!SearchFilePath(psName.c_str(), psPath))
		{
			ELOG("PipelineState::LoadFromJsonMeshShader() : PS not  found %ls", psName.c_str());
			return false;
		}
		ComPtr<ID3DBlob> pPS;
		if (FAILED(D3DReadFileToBlob(psPath.c_str(), pPS.GetAddressOf()))) 
		{ return false; }

		// Amplification Shader
		ComPtr<ID3DBlob> pAS;
		bool hasAS = _json.contains("AS");
		if (hasAS)
		{
			std::wstring asPath;
			std::wstring asName = StringUtil::ToWString(_json.at("AS").get<std::string>());
			if (!SearchFilePath(asName.c_str(), asPath))
			{
				ELOG("PipelineState::LoadFromJsonMeshShader() : AS not found %ls", asName.c_str());
				return false;
			}
			if (FAILED(D3DReadFileToBlob(asPath.c_str(), pAS.GetAddressOf()))) 
			{ return false; }
		}

		// ラスタライザ設定
		D3D12_RASTERIZER_DESC rsDesc = {};
		rsDesc.FillMode					= ToFillMode(_json.value("FillMode", std::string("SOLID")));
		rsDesc.CullMode					= ToCullMode(_json.value("CullMode", std::string("BACK")));
		rsDesc.FrontCounterClockwise	= FALSE;
		rsDesc.DepthBias				= D3D12_DEFAULT_DEPTH_BIAS;
		rsDesc.DepthBiasClamp			= D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rsDesc.SlopeScaledDepthBias		= D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rsDesc.DepthClipEnable			= TRUE;

		// ブレンド設定
		D3D12_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable		= FALSE;
		blendDesc.IndependentBlendEnable	= FALSE;
		const bool blendEnable				= _json.value("blendEnable", false);
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
		dssDesc.DepthEnable		= _json.value("DepthEnable", true) ? TRUE : FALSE;
		dssDesc.DepthWriteMask	= ToDepthWriteMask(_json.value("DepthWriteMask", std::string("ALL")));
		dssDesc.DepthFunc		= ToComparisonFunc(_json.value("DepthFunc", std::string("LESS_EQUAL")));
		dssDesc.StencilEnable	= FALSE;

		// レンダーターゲット
		D3D12_RT_FORMAT_ARRAY rtFormat = {};
		rtFormat.NumRenderTargets	= 1;
		rtFormat.RTFormats[0]		= ToFormat(_json.value("RTVFormat", std::string("R8G8B8A8_UNORM")));

		// サンプラー
		DXGI_SAMPLE_DESC descSample;
		descSample.Count	= 1;
		descSample.Quality	= 0;

		MeshShaderPipelineStateDesc desc = {};

		desc.RootSignature = _pRootSignature;
		if (hasAS) 
		{
			desc.AS = { pAS->GetBufferPointer(), pAS->GetBufferSize() };
		}
		desc.MS				= { pMS->GetBufferPointer(), pMS->GetBufferSize() };
		desc.PS				= { pPS->GetBufferPointer(), pPS->GetBufferSize() };
		desc.Rasterizer		= rsDesc;
		desc.Blend			= blendDesc;
		desc.DepthStencil	= dssDesc;
		desc.SampleMask		= UINT_MAX;
		desc.SampleDesc		= descSample;
		desc.RTFormats		= rtFormat;
		desc.DSFormat		= ToFormat(_json.value("DSVFormat", std::string("D32_FLOAT")));
		desc.Flags			= D3D12_PIPELINE_STATE_FLAG_NONE;

		D3D12_PIPELINE_STATE_STREAM_DESC descStream = {};
		descStream.SizeInBytes = sizeof(desc);
		descStream.pPipelineStateSubobjectStream = &desc;

		// デバイスの生成
		ComPtr<ID3D12Device12> pDevice12;
		auto hr = _pDevice->QueryInterface(IID_PPV_ARGS(pDevice12.GetAddressOf()));
		if (FAILED(hr)) 
		{
			ELOG("PipelineState::LoadFromJsonMeshShader() : QueryInterface(ID3D12Device12)failed hr = 0x%08X", hr);
			return false; 
		}

		// パイプラインステートを生成
		hr = pDevice12->CreatePipelineState(&descStream, IID_PPV_ARGS(m_pPSO.GetAddressOf()));
		if (FAILED(hr))
		{
			ELOG("PipelineState::LoadFromJsonMeshShader() : CreatePipelineState failed hr = 0x%08X", hr);
			return false;
		}
	}
	catch (const std::exception& e)
	{
		ELOG("PipelineState::LoadFromJosnMeshShader() : exception : %s", e.what());
		return false;
	}

	return true;
}
