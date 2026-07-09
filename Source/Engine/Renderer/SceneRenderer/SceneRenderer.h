#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/Renderer/RenderGraph/RenderGraph.h>
#include <Engine/Renderer/PostProcess/PostProcessStack/PostProcessStack.h>
#include <Engine/Renderer/RenderQueue/RenderQueue.h>
#include <Engine/RHI/Pipeline/RootSignature/RootSignatureLayout/RootSignatureLayout.h>
#include <Engine/RHI/Resource/Sampler/Sampler.h>
#include <Engine/RHI/Resource/Buffer/ConstantBuffer/ConstantBuffer.h>
#include <Engine/Mesh/MeshletResource/MeshletResource.h>
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
	// @brief	メッシュ描画用のRootSignatureを取得
	//			MeshComponent::SetRootLayoutに渡すため、GameSceneから参照される
	// -------------------------------------------------------------------------------
	RHI::RootSignatureLayout*	GetMeshRootSignatureLayout()	{ return &m_MeshRootSignatureLayout; }

	// -------------------------------------------------------------------------------
	// @brief	メッシュ描画用のRootSignatureを取得
	//			MeshComponent::SetRootLayoutBindlessに渡すため、GameSceneから参照される
	// -------------------------------------------------------------------------------
	RHI::RootSignatureLayout*	GetMeshRootSignatureLayoutBIndless() { return &m_MeshRootSignatureLayoutBindless; }

	// -------------------------------------------------------------------------------
	// @brief	現在の描画で使っているパイプラインのモードを取得
	// -------------------------------------------------------------------------------
	RenderMode					GetRenderMode()					{ return m_RenderMode; }

private:

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
	RHI::ConstantBuffer			testIndexCB;

	// MeshShader三角形テスト
	RHI::RootSignatureLayout	m_TriangleRootSignatureLayout;
	ID3D12PipelineState*		m_pTrianglePSO = nullptr;
	MeshletResource				m_TriangleMesh;
	RHI::ConstantBuffer			m_TriangleTransformCB;

};