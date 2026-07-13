#pragma once
// -------------------------------------------------------------------------------
// 前方宣言
// -------------------------------------------------------------------------------
class MeshletRenderQueue;

// -------------------------------------------------------------------------------
// IMeshletRenderable interface
// 
// 概要 : 
//	MeshShaderパイプラインで描画するコンポーネントが実装するインターフェース
//	IRenderableのMeshletRenderQueue番
// -------------------------------------------------------------------------------
class IMeshletRenderable
{
public:

	virtual ~IMeshletRenderable() = default;

	// @brief	MeshletRenderQueueへ描画コマンドを積む
	virtual void Submit(MeshletRenderQueue* _pQueue) = 0;

	// @brief	描画が有効かどうかを返す
	virtual bool IsVisible() const = 0;

};
