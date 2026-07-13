#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/GameObject/Component/Component.h>
#include <Engine/GameObject/Renderable/IMeshletRenderable.h>
#include <Engine/Mesh/MeshletResource/MeshletResource.h>
#include <Engine/Mesh/Material/Material.h>
#include <Engine/RHI/Resource/Buffer/ConstantBuffer/ConstantBuffer.h>

// -------------------------------------------------------------------------------
// 前方宣言
// -------------------------------------------------------------------------------
namespace RHI
{
	class Device;
	class RootSignatureLayout;
}

// -------------------------------------------------------------------------------
// ModelMeshletEntry
// 
// 概要 : 
//	メッシュレットのモデル描画で使うメッシュ1個分のデータを詰める構造体
// -------------------------------------------------------------------------------
struct ModelMeshletEntry
{
	std::unique_ptr<MeshletResource> Mesh;
	uint32_t DiffuseTextureIndex = 0;
};

// -------------------------------------------------------------------------------
// MeshletComponent class
// 
// 概要 : 
//	MeshShaderパイプラインでの描画を担当するコンポーネント
//	ComponentとIMeshletRenderableを継承する
// 
//	モデルのロードからメッシュレット分割まで行う（今は）
// -------------------------------------------------------------------------------
class MeshletComponent : public Component, public IMeshletRenderable
{
public:

	// 定数バッファ（ワールド・ビュー・プロジェクション）
	// MeshComponent自身が持ち、TrnsformComponentから毎フレーム更新する
	struct alignas(256) TransformCB
	{
		DirectX::XMMATRIX World;
		DirectX::XMMATRIX View;
		DirectX::XMMATRIX Proj;
	};

	// -------------------------------------------------------------------------------
	// コンストラクタ
	// -------------------------------------------------------------------------------
	MeshletComponent();

	// -------------------------------------------------------------------------------
	// デストラクタ
	// -------------------------------------------------------------------------------
	~MeshletComponent();

	// -------------------------------------------------------------------------------
	// @brief	モデルをロードし、メッシュレット化、マテリアル生成、定数バッファ確保まで行う
	// 
	// @param[in]	_pDevice	デバイス
	// @param[in]	_modelPath	ロードするモデルのファイルパス
	// @retval	true	成功
	// @retval	false	失敗
	// -------------------------------------------------------------------------------
	bool Init(
		RHI::Device*			_pDevice,
		const std::wstring&		_modelPath);

	// -------------------------------------------------------------------------------
	// コンポーネントインターフェースの実装
	// -------------------------------------------------------------------------------
	void Update(float _deltaTime)	override {}
	void OnDetach()					override;


	// -------------------------------------------------------------------------------
	// IMeshRenderableインターフェースの実装
	// -------------------------------------------------------------------------------

	// @brief	描画コマンドを積む
	void Submit(MeshletRenderQueue* _pQueue) override;
	// @brief	描画が有効かどうか返す
	bool IsVisible() const override;

	// @brief	現在のフレームインデックスを設定する
	void SetFrameIndex(uint32_t _frameIndex);

	// @brief	RootSignatureLayoutを設定する（スロット番号をここから取得する）
	void SetRootLayout(const RHI::RootSignatureLayout* _pRootLayout);

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

private:

	// -------------------------------------------------------------------------------
	// private variables
	// -------------------------------------------------------------------------------
	std::vector<ModelMeshletEntry>						m_Meshes;
	std::vector<std::unique_ptr<Material>>				m_Materials;
	std::vector<std::unique_ptr<RHI::ConstantBuffer>>	m_TransformCBs;
	uint32_t											m_FrameIndex	= 0;

	// カメラ行列（GameSceneから毎フレーム更新）
	DirectX::XMMATRIX m_View = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX m_Proj = DirectX::XMMatrixIdentity();

	// RootSignatureのスロット番号（GameSceneの設定と合わせる）
	uint32_t m_TransformSlot	= UINT32_MAX;
	uint32_t m_TextureIndexSlot	= UINT32_MAX;

	bool m_IsVisible = true;

	MeshletComponent(const MeshletComponent&) = delete;
	void operator = (const MeshletComponent&) = delete;
};
