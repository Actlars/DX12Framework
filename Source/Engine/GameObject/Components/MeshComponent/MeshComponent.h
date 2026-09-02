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

	// -------------------------------------------------------------------------------
	// 描くモデルの指定
	//
	// 考え方 :
	//	コンポーネントが持つのは「どのモデルの何番目を描きたいか」という希望だけで、
	//	実際にGPUリソースを結び付ける作業は行わない
	//	GPUリソースの取得にはデバイスとModelLibraryが必要で、
	//	それらを知っているのはシーン側だけだからである
	//
	//	シーンは毎フレーム NeedsModelUpdate() を見て、
	//	ずれているものだけ ApplyModel() で実体を結び付ける
	//
	//	この分け方により
	//		・コンポーネントは保存・復元できる「ただの値」に保てる
	//		・エディタから割り当てても、シーンから割り当てても同じ経路になる
	//		・モデルがまだ読み込まれていなくても、あとから解決される
	//	という状態になる
	// -------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------
	// @brief	描きたいモデルを指定する（この時点では見た目は変わらない）
	//
	// @param[in]	_modelKey	プロジェクトからの相対パス。空なら「モデル無し」
	// @param[in]	_partIndex	モデル内の何番目のメッシュを描くか
	// -------------------------------------------------------------------------------
	void SetModelRequest(std::string_view _modelKey, uint32_t _partIndex = 0);

	const std::string&	GetModelKey()	const { return m_ModelKey; }
	uint32_t			GetPartIndex()	const { return m_PartIndex; }

	// @brief	希望と実際の割り当てがずれているか（シーンが毎フレーム見る）
	bool NeedsModelUpdate() const
	{
		return m_ModelKey != m_AppliedModelKey || m_PartIndex != m_AppliedPartIndex;
	}

	// -------------------------------------------------------------------------------
	// @brief	希望どおりのモデルを実際に結び付ける（シーンから呼ぶ）
	//
	// @param[in]	_pMesh		割り当てるメッシュ。nullptrなら「無し」にする
	// @param[in]	_pMaterial	割り当てるマテリアル
	// -------------------------------------------------------------------------------
	void ApplyModel(Mesh* _pMesh, Material* _pMaterial);

	// -------------------------------------------------------------------------------
	// @brief	定数バッファの用意が済んでいるか
	//
	//	エディタから追加された直後のコンポーネントはまだ済んでいない
	//	シーンがこれを見て、必要なものだけInitする
	// -------------------------------------------------------------------------------
	bool IsReady() const { return !m_TransformCBs.empty(); }

private:

	// -------------------------------------------------------------------------------
	// private variables
	// -------------------------------------------------------------------------------

	// 描画リソース（所有権なし）
	Mesh*		m_pMesh		= nullptr;
	Material*	m_pMaterial = nullptr;

	// -------------------------------------------------------------------------------
	// 描きたいモデル（希望）と、いま実際に結び付いているモデル
	//
	//	2つを比べることで「まだ反映されていない」ことが分かる
	//	保存されるのは希望のほうだけでよい
	// -------------------------------------------------------------------------------
	std::string	m_ModelKey;
	uint32_t	m_PartIndex			= 0;

	std::string	m_AppliedModelKey;
	uint32_t	m_AppliedPartIndex	= 0;

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
