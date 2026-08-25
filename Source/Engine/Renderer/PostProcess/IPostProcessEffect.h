#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/Renderer/RenderGraph/RenderGraph.h>
#include <Engine/Renderer/SceneOutput/SceneOutput.h>

namespace RHI { class Device; }

// -------------------------------------------------------------------------------
// IPostProcessEffect class Interface
// 
// 概要 : 
//	1つのポストエフェクトを表現する。
//	自分に必要なRootSignature / PSO / リソースを自分で保持し
//	AddPassesでRenderGraphに自分のパスを積む
// -------------------------------------------------------------------------------
class IPostProcessEffect
{
public:

	virtual ~IPostProcessEffect() = default;

	// -------------------------------------------------------------------------------
	// @brief	RootSignature / PSO / 定数バッファ等、エフェクト固有のリソースを初期化
	// -------------------------------------------------------------------------------
	virtual bool Init(RHI::Device* _pDevice) = 0;

	// -------------------------------------------------------------------------------
	// @brief	自分のパスをRenderGraphに積む
	// 
	// @param[in]		_graph		パスの登録先
	// @param[in,out]	_sceneColor	現在の「シーンの絵」を指すハンドル
	//								このエフェクトが絵を変化させた場合
	//								自分の出力ハンドルに書き換えて返す
	// @param[in]		_backBuffer	最終出力先のハンドル（最後のエフェクトだけが書く）
	// @param[in]		_isLast		自分が最後のエフェクトかどうか確認
	// @param[in]		_output		最終出力先の実体（RTVと大きさ）
	//								バックバッファ固定ではなくなったため、
	//								デバイスから引かずにここで受け取る
	// -------------------------------------------------------------------------------
	virtual void AddPasses(
		RG::RenderGraph&	_graph,
		RG::Handle&			_sceneColor,
		RG::Handle			_backBuffer,
		bool				_isLast,
		const SceneOutput&	_output) = 0;
};
