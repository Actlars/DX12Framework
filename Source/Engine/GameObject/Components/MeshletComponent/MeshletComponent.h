#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/GameObject/Component/Component.h>
#include <Engine/GameObject/Renderable/IMeshletRenderable.h>
#include <Engine/Mesh/MeshletResource/MeshletResource.h>
#include <Engine/Mesh/Material/Material.h>
#include <Engine/RHI/Resource/Buffer/ConstantBuffer/ConstantBuffer.h>
#include <Engine/Renderer/RendererItem/TransformCB.h>

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

	// -------------------------------------------------------------------------------
	// @brief	読み込み済みのメッシュ数
	//
	//	IsVisible()は「表示フラグ」と「メッシュの有無」の両方を含むため、
	//	エディタが「まだモデルが無い」ことだけを知りたい場合はこちらを見る
	// -------------------------------------------------------------------------------
	size_t GetMeshCount() const { return m_Meshes.size(); }

	// -------------------------------------------------------------------------------
	// 描くモデルの指定
	//
	//	MeshComponentと同じ考え方で、ここには希望だけを記録する
	//	メッシュレット化はモデルの読み込みからやり直す必要があり、
	//	デバイスを持つシーン側でしか行えないため
	// -------------------------------------------------------------------------------
	void SetModelRequest(std::string_view _modelKey);

	const std::string& GetModelKey() const { return m_ModelKey; }

	// @brief	希望と実際の読み込み内容がずれているか（シーンが毎フレーム見る）
	bool NeedsModelUpdate() const { return m_ModelKey != m_AppliedModelKey; }

	// -------------------------------------------------------------------------------
	// @brief	希望どおりのモデルを読み直す（シーンから呼ぶ）
	//
	//	読み込み済みのものはいったん解放してから読み直す
	//
	// @param[in]	_pDevice		GPUリソースの生成に使うデバイス
	// @param[in]	_absolutePath	読み込むモデルの絶対パス
	// @return	true : 読み込めた
	// -------------------------------------------------------------------------------
	bool ApplyModel(RHI::Device* _pDevice, const std::wstring& _absolutePath);

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

	// 描きたいモデル（希望）と、いま実際に読み込んでいるモデル
	std::string m_ModelKey;
	std::string m_AppliedModelKey;

	bool m_IsVisible = true;

	MeshletComponent(const MeshletComponent&) = delete;
	void operator = (const MeshletComponent&) = delete;
};
