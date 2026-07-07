#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/Renderer/RenderGraph/RenderGraph.h>
#include <Engine/Renderer/PostProcess/PostProcessStack/PostProcessStack.h>
#include <Engine/Renderer/RenderQueue/RenderQueue.h>
#include <Engine/RHI/Pipeline/RootSignature/RootSignatureLayout/RootSignatureLayout.h>
#include <Engine/RHI/Resource/Sampler/Sampler.h>
#include <Engine/Mesh/ResData.h>

namespace RHI { class Device; }
class GameObjectManager;
class FPSCamera;

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
	// @brief	メッシュ描画用のRootSignatureを取得
	//			MeshComponent::SetRootLayoutに渡すため、GameSceneから参照される
	// -------------------------------------------------------------------------------
	RHI::RootSignatureLayout* GetMeshRootSignatureLayout() { return &m_MeshRootSignatureLayout; }

private:

	RHI::Device*				m_pDevice = nullptr;
	RHI::RootSignatureLayout	m_MeshRootSignatureLayout;
	ID3D12PipelineState*		m_pMeshPSO = nullptr;
	RG::RenderGraph				m_RenderGraph;
	PostProcessStack			m_PostProcessStack;
	RenderQueue					m_RenderQueue;
	RHI::Sampler				m_Sampler;

};