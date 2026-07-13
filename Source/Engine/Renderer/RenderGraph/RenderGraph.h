#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "RGResourceRegistry.h"
#include "RGRenderPass.h"
#include <Engine/RHI/Resource/ResourceStateTracker/ResourceStateTracker.h>

namespace RG
{
	// -------------------------------------------------------------------------------
	// RenderGraph class
	// 
	// 概要 : 
	//	複数のRenderPassを登録順に実行する
	//	各パスのSetupで宣言されたリソースの使用情報をもとに
	//	実行直前に必要なバリアだけを自動で発行する
	// -------------------------------------------------------------------------------
	class RenderGraph
	{
	public:

		// -------------------------------------------------------------------------------
		// @brief	外部リソースをグラフに取り込む。フレームの最初に呼ぶ
		// -------------------------------------------------------------------------------
		Handle ImportResource(const std::string& _name, ID3D12Resource* _pResource);

		// -------------------------------------------------------------------------------
		// @brief	パスを登録する
		// -------------------------------------------------------------------------------
		void AddPass(
			const std::string& _name,
			std::function<void(PassBuilder&)> _setup,
			std::function<void(ID3D12GraphicsCommandList*, const ResourceRegistry&)> _execute);

		// -------------------------------------------------------------------------------
		// @brief	登録されたパスを順番に実行する
		//			各パスの直前に、そのパスが宣言したリソース使用に応じたバリアを発行する
		// 
		// @param[in]	_pCmd		コマンドリスト
		// @param[in]	_pTracker	リソースステート管理者（Deviceが持つもの）
		// -------------------------------------------------------------------------------
		void Execute(ID3D12GraphicsCommandList* _pCmd, RHI::ResourceStateTracker* _pTracker);


		ResourceRegistry& GetRegistry() { return m_ResourceRegistry; }

	private:

		ResourceRegistry		m_ResourceRegistry;
		std::vector<RenderPass> m_RenderPasses;
	};
}
