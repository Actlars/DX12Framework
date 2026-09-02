#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/Renderer/RenderGraph/RenderGraph.h>
#include <Engine/Renderer/SceneOutput/SceneOutput.h>
#include <Engine/Renderer/PostProcess/PostProcessStack/PostProcessStack.h>
#include <Engine/Renderer/RenderQueue/RenderQueue.h>
#include <Engine/Renderer/RenderQueue/MeshletRenderQueue/MeshletRenderQueue.h>
#include <Engine/Renderer/RenderQueue/DebugLineRenderQueue/DebugLineRenderQueue.h>
#include <Engine/Renderer/RendererItem/DebugLineDrawItem/DebugLineDrawItem.h>
#include <Engine/RHI/Pipeline/RootSignature/RootSignatureLayout/RootSignatureLayout.h>
#include <Engine/RHI/Resource/Sampler/Sampler.h>
#include <Engine/RHI/Resource/Buffer/ConstantBuffer/ConstantBuffer.h>
#include <Engine/Mesh/Material/Material.h>
#include <Engine/Mesh/ResData.h>

namespace RHI { class Device; }
class GameObjectManager;
class FPSCamera;

enum class RenderMode
{
	Traditional,	// CPUが呼ぶ方式 : CPUがDrawごとにSetGraphicsRootDescriptorTableでバインド
	Bindless,		// Bindless方式 : ResourceDescriptorHeap[]でシェーダーが自身でリソースを入手
};


// -------------------------------------------------------------------------------
// SceneRenderer
// 
// 概要 : 
//	「シーンの中身をどういう手順で画面に描くか」という描画エンジン側の
//	責務を一手に引き受けるクラス
// -------------------------------------------------------------------------------
class SceneRenderer
{
public:

	// -------------------------------------------------------------------------------
	// コンストラクタ
	// -------------------------------------------------------------------------------
	SceneRenderer();

	// -------------------------------------------------------------------------------
	// デストラクタ
	// -------------------------------------------------------------------------------
	~SceneRenderer();

	// -------------------------------------------------------------------------------
	// @brief	ポストエフェクトを追加
	//			Initで初期化するので、Initより前で呼んでおく
	// -------------------------------------------------------------------------------
	void AddPostProcessEffect(std::unique_ptr<IPostProcessEffect> _effect);

	void SetFullViewport(ID3D12GraphicsCommandList* _pCmd, uint32_t _width, uint32_t _height);

	// -------------------------------------------------------------------------------
	// @brief	描画エンジンとして必要な基盤の初期化
	// -------------------------------------------------------------------------------
	bool Init(RHI::Device* _pDevice);

	// -------------------------------------------------------------------------------
	// @brief	終了処理
	// -------------------------------------------------------------------------------
	void Term();

	// -------------------------------------------------------------------------------
	// @brief	1フレーム分の描画を行う
	// 
	// @param[in]	_pCmd		コマンドリスト
	// @param[in]	_objects	描画対象一式
	// @param[in]	_camera		描画に使うカメラ
	// -------------------------------------------------------------------------------
	void Render(ID3D12GraphicsCommandList* _pCmd, GameObjectManager& _objects, FPSCamera& _camera);

	// -------------------------------------------------------------------------------
	// @brief	描画で使うパイプラインの切り替え
	// -------------------------------------------------------------------------------
	void SetRenderMode(RenderMode _mode) { m_RenderMode = _mode; }

	// -------------------------------------------------------------------------------
	// @brief	シーンの出力先を差し替える
	//
	//	既定ではバックバッファへ描くが、エディタのビューポートへ表示する場合は
	//	オフスクリーンのレンダーターゲットを指定する
	//	無効なSceneOutputを渡すと、バックバッファへ戻る
	// -------------------------------------------------------------------------------
	void SetOutputTarget(const SceneOutput& _output) { m_OutputOverride = _output; }
	void ClearOutputTarget() { m_OutputOverride = SceneOutput{}; }

	// -------------------------------------------------------------------------------
	// @brief	メッシュ描画用のRootSignatureを取得
	//			MeshComponent::SetRootLayoutに渡すため、GameSceneから参照される
	// -------------------------------------------------------------------------------
	// -------------------------------------------------------------------------------
	// @brief	今フレーム実際に使う出力先を返す
	//
	//	差し替えが指定されていればそれを、なければバックバッファを返す
	//	「どこへ描くか」の判断をこの1か所に閉じ込めるための関数
	// -------------------------------------------------------------------------------
	SceneOutput ResolveOutputTarget() const;

	RHI::RootSignatureLayout*	GetMeshRootSignatureLayout()	{ return &m_MeshRootSignatureLayout; }
	// -------------------------------------------------------------------------------
	// @brief	メッシュ描画用のRootSignatureを取得
	//			MeshComponent::SetRootLayoutBindlessに渡すため、GameSceneから参照される
	// -------------------------------------------------------------------------------
	RHI::RootSignatureLayout*	GetMeshRootSignatureLayoutBIndless()	{ return &m_MeshRootSignatureLayoutBindless; }

	// MeshletComponent::SetRootLayoutに渡すため、GameSceneから参照される
	RHI::RootSignatureLayout* GetModelMeshletRootSignatureLayout()		{ return &m_ModelMeshletRootSignatureLayout; }

	// -------------------------------------------------------------------------------
	// @brief	現在の描画で使っているパイプラインのモードを取得
	// -------------------------------------------------------------------------------
	RenderMode					GetRenderMode()					{ return m_RenderMode; }

	// @brief	デバッグモード切替
	void ToggleMeshletDebugMode() { m_MeshletDebugMode = m_MeshletDebugMode ? 0 : 1; }

	void SetNTCPreviewTexture(RHI::Texture* _pTexture) { m_pNTCPreviewTexture = _pTexture; }

private:

	// バックバッファ以外へ描きたい場合の指定。無効なら通常どおりバックバッファへ描く
	SceneOutput m_OutputOverride{};


	RenderMode					m_RenderMode	= RenderMode::Bindless;	// 描画モード

	RHI::Device*				m_pDevice = nullptr;
	RHI::RootSignatureLayout	m_MeshRootSignatureLayout;
	RHI::RootSignatureLayout	m_MeshRootSignatureLayoutBindless;
	ID3D12PipelineState*		m_pMeshPSO			= nullptr;
	ID3D12PipelineState*		m_pMeshPSOBindless	= nullptr;
	RG::RenderGraph				m_RenderGraph;
	PostProcessStack			m_PostProcessStack;
	RenderQueue					m_RenderQueue;
	RHI::Sampler				m_Sampler;

	// MeshShader
	// 描画対象データはMeshletComponentが持つ
	RHI::RootSignatureLayout	m_ModelMeshletRootSignatureLayout;
	ID3D12PipelineState*		m_pModelMeshletPSO = nullptr;
	MeshletRenderQueue			m_MeshletRenderQueue;
	uint32_t					m_MeshletDebugMode = 0;

	RHI::RootSignatureLayout m_NTCPreviewRootSignatureLayout;
	ID3D12PipelineState* m_pNTCPreviewPSO = nullptr;
	RHI::Texture* m_pNTCPreviewTexture = nullptr;

	// DebugLinehader
	RHI::RootSignatureLayout	m_DebugLineRootSignatureLayout;
	ID3D12PipelineState*		m_pDebugLinePSO = nullptr;
	DebugLineRenderQueue		m_DebugRenderQueue;

};
