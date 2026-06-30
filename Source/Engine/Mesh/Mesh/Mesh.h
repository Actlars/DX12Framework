#pragma once

// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "../MeshResource.h"
#include "../ResData.h"

// -------------------------------------------------------------------------------
// Mesh class
// 
// 概要 : 
//	1メッシュ分の描画単位
//	MeshResource（VB/IB等）をunique_ptrで持ち
//	マテリアルIDで対応するマテリアルを参照する
// 
//	描画方法（従来パイプライン / メッシュシェーダー）は
//	MeshResourceインターフェース越しに抽象化されており
//	Meshクラス自身は描画方法を意識しない
// 
// 使い方 : 
//	ロード
//	std::vector<ResMesh>		resMeshes;
//	std::vector<ResMaterial>	resMaterials;
//	MeshLoader::Load(L"Assets/Model.gotf", resMeshes, resMaterials);
// 
//	GPUリソース生成
//	std::vector<Mesh> meshes(resMeshes.size());
//	for(auto i = 0u; i < meshes.size(); ++i)
//	{ meshes[i].Init(pDevice, resMeshes[i]); }
// 
//	描画
//	for(auto& mesh : meshes)
//	{
//		auto matId = mesh.GetMaterialId();
//		pCmd->SetGraphicsRootConstantBufferView(0, materials[matId].GetCBAddress());
//		mesh.Draw(pCmd);
//	}
// -------------------------------------------------------------------------------
class Mesh
{
public:

	// -------------------------------------------------------------------------------
	// コンストラクタ
	// -------------------------------------------------------------------------------
	Mesh();

	// -------------------------------------------------------------------------------
	// デストラクタ
	// -------------------------------------------------------------------------------
	~Mesh();

	// -------------------------------------------------------------------------------
	// @brief	従来パイプライン（VB/IB）で初期化
	// 
	// @param[in]	_pDevice	デバイス
	// @param[in]	_resMesh	ロード済みの生メッシュデータ
	// @retval	true	成功
	// @retval	false	失敗
	// -------------------------------------------------------------------------------
	bool Init(ID3D12Device* _pDevice, const ResMesh& _resMesh);

	// -------------------------------------------------------------------------------
	// @brief	任意のMeshResourceで初期化する（メッシュシェーダー等への対応）
	// 
	// @param[in]	_resource	生成済みのMeshResource（所有権を移転する）
	// @param[in]	_materialId	対応するマテリアルのインデックス
	// -------------------------------------------------------------------------------
	void InitWithResource(
		std::unique_ptr<MeshResource>	_resource,
		uint32_t						_materialId);

	// -------------------------------------------------------------------------------
	// @brief	終了処理
	// -------------------------------------------------------------------------------
	void Term();

	// -------------------------------------------------------------------------------
	// @brief	描画コマンドを積む
	// 
	// @param[in]	_pCmd			コマンドリスト
	// @param[in]	_instanceCount	インスタンス数（通常は１）
	// -------------------------------------------------------------------------------
	void Draw(
		ID3D12GraphicsCommandList*	_pCmd,
		uint32_t					_instanceCount = 1) const;

	// -------------------------------------------------------------------------------
	// @brief	InputLayoutが必要かどうかを返す（PSO生成時に参照）
	// -------------------------------------------------------------------------------
	bool NeedsInputLayout()		const;

	// -------------------------------------------------------------------------------
	// @brief	ゲッター
	// -------------------------------------------------------------------------------
	uint32_t GetMaterialId()	const;
	uint32_t GetIndexCount()	const;
	uint32_t GetVertexCount()	const;

	bool IsValid() const;

private:

	std::unique_ptr<MeshResource>	m_pResource;		// 描画リソース
	uint32_t						m_MaterialId = 0;	// マテリアルインデックス

	Mesh			(const Mesh&) = delete;
	void operator = (const Mesh&) = delete;
};

