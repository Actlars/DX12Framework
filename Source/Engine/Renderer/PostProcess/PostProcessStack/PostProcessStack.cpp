// -------------------------------------------------------------------------------
//		Includes
// -------------------------------------------------------------------------------
#include "PostProcessStack.h"

// -------------------------------------------------------------------------------
//		エフェクトの末尾追加
// -------------------------------------------------------------------------------
void PostProcessStack::AddEffect(std::unique_ptr<IPostProcessEffect> _effect)
{
	m_PostProcessEffects.emplace_back(std::move(_effect));
}

// -------------------------------------------------------------------------------
//		全エフェクトの初期化
// -------------------------------------------------------------------------------
bool PostProcessStack::InitAllEffect(RHI::Device* _pDevice)
{
	for (auto& effect : m_PostProcessEffects)
	{
		if (!effect->Init(_pDevice)) { return false; }
	}

	return true;
}

// -------------------------------------------------------------------------------
//		全エフェクトのパスをRenderGraphに順番に積む
// -------------------------------------------------------------------------------
void PostProcessStack::Execute(
	RG::RenderGraph&	_graph,
	RG::Handle&			_sceneColor,
	RG::Handle&			_backBuffer,
	const SceneOutput&	_output)
{
	for (size_t i = 0; i < m_PostProcessEffects.size(); ++i)
	{
		const bool isLast = (i == m_PostProcessEffects.size() - 1);
		m_PostProcessEffects[i]->AddPasses(_graph, _sceneColor, _backBuffer, isLast, _output);
	}
}
