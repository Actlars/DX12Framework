#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/Renderer/PostProcess/IPostProcessEffect.h>
#include <Engine/Renderer/RenderGraph/RenderGraph.h>
#include <Engine/Renderer/RenderGraph/RGTypes.h>

// -------------------------------------------------------------------------------
// PostProcessStack class
// 
// 概要 : 
//	登録されたIPostProcessEffectを順番に実行する
//	GameSceneはこのクラスに対して「エフェクトを1つ足す」操作を行う
// -------------------------------------------------------------------------------
class PostProcessStack
{
public:

	// -------------------------------------------------------------------------------
	// @brief	エフェクトを末尾に追加する。所有権を移す
	// -------------------------------------------------------------------------------
	void AddEffect(std::unique_ptr<IPostProcessEffect> _effect);

	// -------------------------------------------------------------------------------
	// @brief	登録された全エフェクトを初期化する
	// -------------------------------------------------------------------------------
	bool InitAllEffect(RHI::Device* _pDevice);

	// -------------------------------------------------------------------------------
	// @brief	全エフェクトのパスをRenderGraphに順番に積む
	// 
	// @param[in]		_graph		パスの登録先
	// @param[in,out]	_sceneColor	MainPassが描いた絵のハンドル（入力かつ出力）
	// @param[in]		_backBuffer	最終的な出力先
	// -------------------------------------------------------------------------------
	void Execute(
		RG::RenderGraph&	_graph,
		RG::Handle&			_sceneColor,
		RG::Handle&			_backBuffer,
		const SceneOutput&	_output);

private:

	std::vector<std::unique_ptr<IPostProcessEffect>> m_PostProcessEffects;

};
