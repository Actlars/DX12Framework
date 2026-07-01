#include "RootSignatureLayout.h"
#include <Engine/Utility/Debug/Logger/Logger.h>
#include <Engine/Utility/FileUtil/FileUtil.h>

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
			if (n == "ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT")			{ flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;	}
			else if (n == "DENY_VERTEX_SHADER_ROOT_ACCESS")			{ flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;		}
			else if (n == "DENY_PIXEL_SHADER_ROOT_ACCESS")			{ flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;			}
			else if (n == "DENY_GEOMETRY_SHADER_ROOT_ACCESS")		{ flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;		}
			else if (n == "DENY_HULL_SHADER_ROOT_ACCESS")			{ flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;			}
			else if (n == "DENY_DOMAIN_SHADER_ROOT_ACCESS")			{ flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;		}
			else if (n == "ALLOW_STREAM_OUTPUT")					{ flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;					}
			else if (n == "LOCAL_ROOT_SIGNATURE")					{ flags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;					}
			else if (n == "DENY_AMPLIFICATION_SHADER_ROOT_ACCESS")	{ flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS; }
			else if (n == "DENY_MESH_SHADER_ROOT_ACCESS")			{ flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;			}
			else { ELOG("RootSignatureLayout::ToFlags : unknown flag string : %s", n.c_str()); }
		}
		return flags;
	}
}

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
RootSignatureLayout::RootSignatureLayout() 
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
RootSignatureLayout::~RootSignatureLayout()
{ /* DO_NOTHING */ }

bool RootSignatureLayout::LoadFromJson(ID3D12Device * _pDevice, const std::wstring & _jsonPath)
{
	if (_pDevice == nullptr) 
	{ return false; }

	std::ifstream ifs(_jsonPath);
	if (!ifs.is_open())
	{
		ELOG("RootSignatureLayout::LoadFromJson : not implemented yet : %ls", _jsonPath.c_str());
		return false;
	}

	nlohmann::json json;
	try { ifs >> json; }
	catch (const std::exception& e)
	{
		ELOG("RootSignature: Json parse error : %s", e.what());
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
		else if (type == "SRV_Table" || type == "Sampler_Table")
		{
			D3D12_DESCRIPTOR_RANGE range = {};
			range.RangeType							= (type == "SRV_Table")
													? D3D12_DESCRIPTOR_RANGE_TYPE_SRV
													: D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
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

	D3D12_ROOT_SIGNATURE_DESC desc = {};
	desc.NumParameters = static_cast<uint32_t>(rootParams.size());
	desc.pParameters = rootParams.data();
	desc.pStaticSamplers = nullptr;
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

uint32_t RootSignatureLayout::GetSlot(const std::string& _name) const
{
	auto it = m_SlotMap.find(_name);
	if (it == m_SlotMap.end())
	{
		ELOG("RootSignatureLayout : slot %s not found", _name.c_str());
		return UINT32_MAX;
	}
	return it->second;
}

ID3D12RootSignature* RootSignatureLayout::GetRootSignature() const 
{ return m_pRootSignature.Get(); }
