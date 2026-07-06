#include "RenderGraph.h"
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
//		外部リソースをレンダーグラフに取り込む
// -------------------------------------------------------------------------------
RG::Handle RG::RenderGraph::ImportResource(const std::string& _name, ID3D12Resource* _pResource) 
{ return m_ResourceRegistry.Import(_name, _pResource); }

// -------------------------------------------------------------------------------
//		パスを登録する
// -------------------------------------------------------------------------------
void RG::RenderGraph::AddPass(
	const std::string& _name, 
	std::function<void(PassBuilder&)> _setup,
	std::function<void(ID3D12GraphicsCommandList*, const ResourceRegistry&)> _execute)
{
	RenderPass pass;
	pass.Name		= _name;
	pass.Setup		= std::move(_setup);
	pass.Execute	= std::move(_execute);

	m_RenderPasses.emplace_back(std::move(pass));
}

// -------------------------------------------------------------------------------
//		登録されたパスを順番に実行する
// -------------------------------------------------------------------------------
void RG::RenderGraph::Execute(ID3D12GraphicsCommandList* _pCmd, RHI::ResourceStateTracker* _pTracker)
{
	for (auto& pass : m_RenderPasses)
	{
		// Setupフェーズ : このパスが何を使うか宣言させる
		PassBuilder builder;
		if (pass.Setup) 
		{ pass.Setup(builder); }

		// 宣言に基づいてバリアを解決する
		for (auto& usage : builder.GetUsages())
		{
			auto* pResource = m_ResourceRegistry.GetResource(usage.resourceHandle);
			if (pResource == nullptr)
			{
				ELOG("RG::RenderGraph::Execute() pass %s references invalid resource handle(name : %s) ", 
					pass.Name.c_str(), m_ResourceRegistry.GetName(usage.resourceHandle).c_str());
				continue;
			}

			// Transientリソースは生成直後、まだトラッカーに登録されていない可能性がある
			// 未登録なら、生成時の既知の初期ステート（RENDER_TARGET）として登録してから遷移させる
			_pTracker->RegisterResourceIfNeeded(pResource, D3D12_RESOURCE_STATE_RENDER_TARGET);
			_pTracker->TransitionResource(pResource, usage.requiredState);
		}
		_pTracker->FlushBarriers(_pCmd);

		// Executeフェーズ : 実際の描画コマンドを積む
		// パスの中からGetRTV/GetSRVでハンドルを引ける
		if (pass.Execute) { pass.Execute(_pCmd, m_ResourceRegistry); }
	}

	// フレーム末にクリアし、次フレームは新規に組み立てなおす
	m_RenderPasses.clear();
	m_ResourceRegistry.Clear();
}
