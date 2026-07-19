#include "RootSignatureLayout.h"
#include <Engine/Utility/Debug/Logger/Logger.h>
#include <Engine/Utility/FileUtil/FileUtil.h>
#include <Engine/Utility/JsonLoader/JsonLoader.h>

namespace
{
	// -------------------------------------------------------------------------------
	// 可視性文字列 → D3D12_SHADER_VISIBILITY 変換テーブル
	// -------------------------------------------------------------------------------
	D3D12_SHADER_VISIBILITY ToVisibility(const std::string& _s)
	{
		if (_s == "VS") { return D3D12_SHADER_VISIBILITY_VERTEX;		}
		if (_s == "PS") { return D3D12_SHADER_VISIBILITY_PIXEL;			}
		if (_s == "GS") { return D3D12_SHADER_VISIBILITY_GEOMETRY;		}
		if (_s == "HS") { return D3D12_SHADER_VISIBILITY_HULL;			}
		if (_s == "CS") { return D3D12_SHADER_VISIBILITY_ALL;			}
		if (_s == "DS") { return D3D12_SHADER_VISIBILITY_DOMAIN;		}
		if (_s == "MS") { return D3D12_SHADER_VISIBILITY_MESH;			}
		if (_s == "AS") { return D3D12_SHADER_VISIBILITY_AMPLIFICATION; }
		ELOG("RootSignatureLayout::ToVisibility : unknown visibility string : %s", _s.c_str());
		return D3D12_SHADER_VISIBILITY_ALL;
	}

	// -------------------------------------------------------------------------------
	// フラグ文字列 → D3D12_ROOT_SIGNATURE_FLAGS（ビットOR）
	// -------------------------------------------------------------------------------
	D3D12_ROOT_SIGNATURE_FLAGS ToFlags(const std::vector<std::string>& _names)
	{
		D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		for (auto& n : _names)
		{
			if		(n == "ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT")		{ flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;	}
			else if (n == "DENY_VERTEX_SHADER_ROOT_ACCESS")			{ flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;		}
			else if (n == "DENY_PIXEL_SHADER_ROOT_ACCESS")			{ flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;			}
			else if (n == "DENY_GEOMETRY_SHADER_ROOT_ACCESS")		{ flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;		}
			else if (n == "DENY_HULL_SHADER_ROOT_ACCESS")			{ flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;			}
			else if (n == "DENY_DOMAIN_SHADER_ROOT_ACCESS")			{ flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;		}
			else if (n == "ALLOW_STREAM_OUTPUT")					{ flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;					}
			else if (n == "LOCAL_ROOT_SIGNATURE")					{ flags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;					}
			else if (n == "DENY_AMPLIFICATION_SHADER_ROOT_ACCESS")	{ flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS; }
			else if (n == "DENY_MESH_SHADER_ROOT_ACCESS")			{ flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;			}
			else if (n == "CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED")		{ flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;		}	// このRootSignatureを使うシェーダーはディスクリプタヒープを直接インデックスアクセスするという宣言（バインドレス化に必要）
			else if (n == "SAMPLER_HEAP_DIRECTLY_INDEXED")			{ flags |= D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;			}
			else { ELOG("RootSignatureLayout::ToFlags : unknown flag string : %s", n.c_str()); }
		}
		return flags;
	}

	D3D12_FILTER ToFilter(const std::string& _s)
	{
		if (_s == "POINT")			{ return D3D12_FILTER_MIN_MAG_MIP_POINT; }
		if (_s == "LINEAR")			{ return D3D12_FILTER_MIN_MAG_MIP_LINEAR; }
		if (_s == "ANISOTROPIC")	{ return D3D12_FILTER_ANISOTROPIC; }
		ELOG("RootSignatureLayout::ToFilter : unknown value : %s", _s.c_str());
		return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	}

	D3D12_TEXTURE_ADDRESS_MODE ToAddressMode(const std::string& _s)
	{
		if (_s == "WRAP")	{ return D3D12_TEXTURE_ADDRESS_MODE_WRAP; }
		if (_s == "CLAMP")	{ return D3D12_TEXTURE_ADDRESS_MODE_CLAMP; }
		if (_s == "MIRROR") { return D3D12_TEXTURE_ADDRESS_MODE_MIRROR; }
		if (_s == "BORDER") { return D3D12_TEXTURE_ADDRESS_MODE_BORDER; }
		ELOG("RootSignatureLayout::ToAddressMode : unknon value : %s", _s.c_str());
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
}

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
RHI::RootSignatureLayout::RootSignatureLayout() 
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
RHI::RootSignatureLayout::~RootSignatureLayout()
{ /* DO_NOTHING */ }

bool RHI::RootSignatureLayout::LoadFromJson(ID3D12Device * _pDevice, const std::wstring & _jsonPath)
{
	if (_pDevice == nullptr) 
	{ return false; }

	nlohmann::json json;
	if (!JsonLoader::Load(_jsonPath, json))
	{
		ELOG("RootSignatureLayout::LoadFromJson : JsonLoader::Load failed path = %ls", _jsonPath.c_str());
		return false;
	}

	const auto& paramsJson = json.at("Parameters");

	// DescriptorRangeはD3D12_ROOT_PARAMETERがポインタで参照するため、
	// vectorの再確保でポインタが無効になるのを防ぐため、先にreserveでDescriptorRangeを確保しておく
	std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
	ranges.reserve(paramsJson.size());

	std::vector<D3D12_ROOT_PARAMETER> rootParams;
	rootParams.reserve(paramsJson.size());

	m_SlotMap.clear();

	for (size_t i = 0; i < paramsJson.size(); ++i)
	{
		const auto& p			= paramsJson[i];
		const std::string name	= p.at("Name").get<std::string>();
		const std::string type	= p.at("Type").get<std::string>();
		const uint32_t reg		= p.at("ShaderRegister").get<uint32_t>();
		const uint32_t space	= p.value("RegisterSpace", 0u);
		const auto visibility	= ToVisibility(p.at("Visibility").get<std::string>());

		D3D12_ROOT_PARAMETER param = {};
		param.ShaderVisibility = visibility;

		if (type == "CBV")
		{
			param.ParameterType				= D3D12_ROOT_PARAMETER_TYPE_CBV;
			param.Descriptor.ShaderRegister = reg;
			param.Descriptor.RegisterSpace	= space;
		}
		else if (type == "SRV")	// Root Descriptor形式のSRV
		{
			param.ParameterType				= D3D12_ROOT_PARAMETER_TYPE_SRV;
			param.Descriptor.ShaderRegister = reg;
			param.Descriptor.RegisterSpace	= space;
		}
		else if (type == "UAV")	// ComputeShader用
		{
			param.ParameterType				= D3D12_ROOT_PARAMETER_TYPE_UAV;
			param.Descriptor.ShaderRegister = reg;
			param.Descriptor.RegisterSpace	= space;
		}
		else if (type == "Constants")
		{
			param.ParameterType				= D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			param.Constants.ShaderRegister	= reg;
			param.Constants.RegisterSpace	= space;
			param.Constants.Num32BitValues	= p.value("Num32BitValues", 1u);
		}
		else if (type == "SRV_Table" || type == "Sampler_Table" || type == "UAV_Table" || type == "CBV_Table")
		{
			D3D12_DESCRIPTOR_RANGE range = {};
			if (type == "SRV_Table")
			{
				range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			}
			else if (type == "Sampler_Table")
			{
				range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			}
			else if (type == "UAV_Table")
			{
				range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			}
			else if (type == "CBV_Table")
			{
				range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			}
			range.NumDescriptors					= p.value("NumDescriptors", 1u);
			range.BaseShaderRegister				= reg;
			range.RegisterSpace						= space;
			range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges.emplace_back(range);	// reserveしているのでポインタは無効にならない

			param.ParameterType							= D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			param.DescriptorTable.NumDescriptorRanges	= 1;
			param.DescriptorTable.pDescriptorRanges		= &ranges.back();
		}
		else
		{
			ELOG("RootSignatureLayout::LoadFromJson : unknown parameter type : %s", type.c_str());
			return false;
		}

		rootParams.emplace_back(param);
		m_SlotMap[name] = static_cast<uint32_t>(i);	// 配列の並び順 = スロット番号
	}

	// Sampler設定
	std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;

	if (json.contains("StaticSamplers"))
	{
		for (const auto& s : json.at("StaticSamplers"))
		{
			D3D12_STATIC_SAMPLER_DESC sampleDesc = {};
			sampleDesc.Filter			= ToFilter(s.value("Filter", std::string("LINEAR")));
			sampleDesc.AddressU			= ToAddressMode(s.value("AddressMode", std::string("WRAP")));
			sampleDesc.AddressV			= sampleDesc.AddressU;
			sampleDesc.AddressW			= sampleDesc.AddressU;
			sampleDesc.MipLODBias		= 0.0f;
			sampleDesc.MaxAnisotropy	= 16;
			sampleDesc.ComparisonFunc	= D3D12_COMPARISON_FUNC_NEVER;
			sampleDesc.BorderColor		= D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			sampleDesc.MinLOD			= 0.0f;
			sampleDesc.MaxLOD			= D3D12_FLOAT32_MAX;
			sampleDesc.ShaderRegister	= s.at("ShaderRegister").get<uint32_t>();
			sampleDesc.RegisterSpace	= s.value("RegisterSpace", 0u);
			sampleDesc.ShaderVisibility = ToVisibility(s.at("Visibility").get<std::string>());

			staticSamplers.emplace_back(sampleDesc);
		}
	}

	D3D12_ROOT_SIGNATURE_DESC desc = {};
	desc.NumParameters = static_cast<uint32_t>(rootParams.size());
	desc.pParameters = rootParams.data();
	desc.NumStaticSamplers = static_cast<uint32_t>(staticSamplers.size());
	desc.pStaticSamplers = staticSamplers.empty() ? nullptr : staticSamplers.data();
	desc.Flags = ToFlags(json.value("Flags", std::vector<std::string>{}));

	ComPtr<ID3DBlob> pBlob, pError;
	auto hr = D3D12SerializeRootSignature(
		&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, 
		pBlob.GetAddressOf(), pError.GetAddressOf());
	if (FAILED(hr)) 
	{
		if (pError) { ELOG("RootSignature serialize error %s", (char*)pError->GetBufferPointer()); }
		return false;
	}

	hr = _pDevice->CreateRootSignature(
		0, 
		pBlob->GetBufferPointer(), pBlob->GetBufferSize(),
		IID_PPV_ARGS(m_pRootSignature.GetAddressOf()));
	if (FAILED(hr)) 
	{
		ELOG("CreateRootSignature failed");
		return false; 
	}

	return true;
}

uint32_t RHI::RootSignatureLayout::GetSlot(const std::string& _name) const
{
	auto it = m_SlotMap.find(_name);
	if (it == m_SlotMap.end())
	{
		ELOG("RootSignatureLayout : slot %s not found", _name.c_str());
		return UINT32_MAX;
	}
	return it->second;
}

ID3D12RootSignature* RHI::RootSignatureLayout::GetRootSignature() const 
{ return m_pRootSignature.Get(); }
