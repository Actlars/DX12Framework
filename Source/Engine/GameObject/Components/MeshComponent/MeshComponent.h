#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/GameObject/Component/Component.h>
#include <Engine/GameObject/Renderable/IRenderable.h>
#include <Engine/Mesh/Mesh/Mesh.h>
#include <Engine/Mesh/Material/Material.h>
#include <Engine/RHI/Resource/Buffer/ConstantBuffer/ConstantBuffer.h>
#include <Engine/Renderer/RendererItem/TransformCB.h>

// -------------------------------------------------------------------------------
// 前方宣言
// -------------------------------------------------------------------------------
namespace RHI
{
	class RootSignatureLayout;
}

// -------------------------------------------------------------------------------
// MeshComponent class
// 
// 概要 : 
//	メッシュの描画を担当するコンポーネント
//	ComponentとIRenderableを継承する
// 
//	TransformComponentからワールド変換行列を取得して定数バッファに渡し、Mesh::Drawで描画コマンドに積む
// -------------------------------------------------------------------------------
class MeshComponent : public Component, public IRenderable
{
public:

	// -------------------------------------------------------------------------------
	// コンストラクタ
	// -------------------------------------------------------------------------------
	MeshComponent();

	// -------------------------------------------------------------------------------
	// デストラクタ
	// -------------------------------------------------------------------------------
	~MeshComponent();

	// -------------------------------------------------------------------------------
	// @brief	定数バッファを初期化
	//			AddComponent後にGameSceneから呼ぶ
	// 
	// @param[in]	_pDevice	デバイス
	// @param[in]	_pPool		CBV用DescriptorPool
	// @param[in]	_frameCount	フレームバッファ数（GraphicsDevice::GetFrameCount())
	// @retval	true	成功
	// @retval	false	失敗
	// -------------------------------------------------------------------------------
	bool Init(
		ID3D12Device*			_pDevice,
		RHI::DescriptorPool*	_pPool,
		uint32_t				_frameCount);

	// -------------------------------------------------------------------------------
	// コンポーネントインターフェースの実装
	// -------------------------------------------------------------------------------
	void Update(float _deltaTime)	override {}
	void OnDetach()					override;


	// -------------------------------------------------------------------------------
	// IRenderableインターフェースの実装
	// -------------------------------------------------------------------------------

	// @brief	描画コマンドを積む
	void Submit(RenderQueue* _pQueue) override;
	//void Draw(ID3D12GraphicsCommandList* _pCmd) override;

	// @brief	描画が有効かどうか返す
	bool IsVisible() const override;

	// -------------------------------------------------------------------------------
	// @brief	描画するメッシュとマテリアルを設定する
	// 
	// @param[in]	_pMesh		Meshへのポインタ（所有権なし）
	// @param[in]	_pMaterial	Materialへのポインタ（所有権なし）
	// -------------------------------------------------------------------------------
	void SetMesh(Mesh* _pMesh, Material* _pMaterial);

	// -------------------------------------------------------------------------------
	// @brief	定数バッファのスロット番号を設定する
	//			GameSceneのRootSignatureと合わせる
	// -------------------------------------------------------------------------------
	void SetRootParamSlots(
		uint32_t _transformSlot,
		uint32_t _materialSlot,
		uint32_t _textureSlot);

	// @brief	現在のフレームインデックスを設定する
	void SetFrameIndex(uint32_t _frameIndex);

	// @brief	RootSignatureLayoutを設定する
	void SetRootLayout(const RHI::RootSignatureLayout* _pRootLayout);

	// @brief	RootSignatureLayoutを設定する（BIndless）
	void SetRootLayoutBindless(const RHI::RootSignatureLayout* _pRootLayout);

	// -------------------------------------------------------------------------------
	// @brief	カメラの View / Proj行列を設定する
	//			GameSceneから毎フレーム渡す
	// -------------------------------------------------------------------------------
	void SetViewProj(
		const DirectX::XMMATRIX& _View,
		const DirectX::XMMATRIX& _Proj);

	// -------------------------------------------------------------------------------
	// @brief	描画の有効・無効を切り替える
	// -------------------------------------------------------------------------------
	void SetVisible(bool _visible);

	// -------------------------------------------------------------------------------
	// @brief	描画するメッシュが設定されているか
	//
	//	IsVisible()は「表示フラグ」と「メッシュの有無」の両方を含むため、
	//	エディタが「まだメッシュが無い」ことだけを知りたい場合はこちらを見る
	// -------------------------------------------------------------------------------
	bool HasMesh() const { return m_pMesh != nullptr; }

private:

	// -------------------------------------------------------------------------------
	// private variables
	// -------------------------------------------------------------------------------

	// 描画リソース（所有権なし）
	Mesh*		m_pMesh		= nullptr;
	Material*	m_pMaterial = nullptr;

	// 定数バッファ
	std::vector<std::unique_ptr<RHI::ConstantBuffer>>	m_TransformCBs;
	std::vector<std::unique_ptr<RHI::ConstantBuffer>>	m_MaterialIndicesCBs;
	uint32_t											m_FrameIndex = 0;

	// カメラ行列（GameSceneから毎フレーム更新）
	DirectX::XMMATRIX m_View = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX m_Proj = DirectX::XMMatrixIdentity();

	// RootSignatureのスロット番号（GameSceneの設定と合わせる）
	uint32_t m_TransformSlot	= UINT32_MAX;
	uint32_t m_MaterialSlot		= UINT32_MAX;
	uint32_t m_TextureSlot		= UINT32_MAX;

	uint32_t m_MaterialIndicesSlot = UINT32_MAX;	// Bindless用

	bool m_IsVisible = true;
};
