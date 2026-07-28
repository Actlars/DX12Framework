#pragma once

namespace RHI
{
	// -------------------------------------------------------------------------------
	// PipelineState class
	// 
	// 概要 : 
	//	JSONファイルからD3D12_GRAPHICS_PIPELINE_STATE_DESCを構築し、
	//	PSOを生成する
	// -------------------------------------------------------------------------------
	class PipelineState
	{
	public:

		// -------------------------------------------------------------------------------
		// @biref	JSONファイルからPSOを生成する
		//
		// @param[in]	_pDevice			デバイス
		// @param[in]	_jsonPath			JSONファイルパス
		// @param[in]	_pRootSignature		紐づけるRootSignature
		// @param[in]	_pInputLayout		頂点入力レイアウト（メッシュ形式ごとに異なるため外部から渡す）
		// @param[in]	_pInputLayoutCount	頂点入力レイアウトの要素数
		// -------------------------------------------------------------------------------
		bool LoadFromJson(
			ID3D12Device*					_pDevice,
			const std::wstring&				_jsonPath,
			ID3D12RootSignature*			_pRootSignature,
			const D3D12_INPUT_LAYOUT_DESC&  _inputLayout);

		// -------------------------------------------------------------------------------
		// @brief	パイプラインステートを取得
		// -------------------------------------------------------------------------------
		ID3D12PipelineState* GetPSO() const { return m_pPSO.Get(); }

	private:

		// -------------------------------------------------------------------------------
		// @brief	VS + PS 用のPSO生成
		// -------------------------------------------------------------------------------
		bool LoadFromJsonGraphics(
			ID3D12Device*					_pDevice,
			const nlohmann::json&			_json,
			ID3D12RootSignature*			_pRootSignature,
			const D3D12_INPUT_LAYOUT_DESC&	_inputLayout);

		// -------------------------------------------------------------------------------
		// @brief	MeshShaderパイプライン用のPSO生成
		//			PipelineStateStream経由で生成
		// -------------------------------------------------------------------------------
		bool LoadFromJsonMeshShader(
			ID3D12Device*			_pDevice,
			const nlohmann::json&	_json,
			ID3D12RootSignature*	_pRootSignature);

		// -------------------------------------------------------------------------------
		// @brief	ComputeShaderパイプライン用のPSO生成
		// -------------------------------------------------------------------------------
		bool LoadFromJsonCompute(
			ID3D12Device*			_pDevice,
			const nlohmann::json&	_json,
			ID3D12RootSignature*	_pRootSignature);

		ComPtr<ID3D12PipelineState> m_pPSO;
	};
}