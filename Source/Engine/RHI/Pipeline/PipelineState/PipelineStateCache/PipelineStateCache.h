#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/RHI/Pipeline/PipelineState/PipelineState.h>

namespace RHI
{
	// -------------------------------------------------------------------------------
	// PipelineStateCache class
	// 
	// 概要 : 
	//	JSONパスをキーとしてPSOをキャッシュする（Flywightパターン）
	//	同じJSONを指定した場合、二回目以降はコンパイル済みのPSOを再利用する
	// -------------------------------------------------------------------------------
	class PipelineCache
	{
	public:

		// -------------------------------------------------------------------------------
		// @brief	キャッシュにあれば返し、なければJSONから生成してキャッシュに登録する
		// -------------------------------------------------------------------------------
		ID3D12PipelineState* GetOrCreate(
			ID3D12Device*					_pDevice,
			const std::wstring&				_jsonPath,
			ID3D12RootSignature*			_pRootSignature,
			const D3D12_INPUT_LAYOUT_DESC&  _inputLayout)
		{
			auto it = m_Cache.find(_jsonPath);
			if (it != m_Cache.end()) 
			{ return it->second->GetPSO(); }

			auto pso = std::make_unique<PipelineState>();
			if (!pso->LoadFromJson(_pDevice, _jsonPath, _pRootSignature, _inputLayout)) 
			{ return nullptr; }

			auto* ptr = pso->GetPSO();
			m_Cache[_jsonPath] = std::move(pso);
			return ptr;
		}

	private:

		std::unordered_map<std::wstring, std::unique_ptr<PipelineState>> m_Cache;

	};
}
