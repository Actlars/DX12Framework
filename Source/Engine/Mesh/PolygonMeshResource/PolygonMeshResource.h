#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "../MeshResource.h"
#include "../ResData.h"

// -------------------------------------------------------------------------------
// PolygonMeshResource class
// 
// 概要 : 
//	従来のInputAssemblerパイプライン（VB/IB）を使ったMeshResource実装
//	ResMeshを受け取り、VertexBufferとIndexBufferをUPLOADヒープに生成
//	Draw()はIASetVertexBuffers / IASetIndexBuffer / DrawIndexedInstancedを呼ぶ
//	
// メッシュシェーダーへの移行 : 
//	MeshletResourceを新たに実装し、Meshクラスが保持するポインタを
//	PolygonMeshResourceからMeshletResourceに差し替えるだけで移行できる
// -------------------------------------------------------------------------------
class PolygonMeshResource final : public MeshResource
{
public:

	// -------------------------------------------------------------------------------
	// コンストラクタ
	// -------------------------------------------------------------------------------
	PolygonMeshResource();

	// -------------------------------------------------------------------------------
	// デストラクタ
	// -------------------------------------------------------------------------------
	~PolygonMeshResource();

	// -------------------------------------------------------------------------------
	// @brief	初期化（ResMeshからGPUリソースを生成）
	// 
	// @param[in]	_pDevice	デバイス
	// @param[in]	_resMesh	ロード済みの生メッシュデータ
	// @retval	true	成功
	// @retval	false	失敗
	// -------------------------------------------------------------------------------
	bool Init(ID3D12Device* _pDevice, const ResMesh& _resMesh);

	// -------------------------------------------------------------------------------
	// @brief	終了処理
	// -------------------------------------------------------------------------------
	void Term();

	// MeshResourceインターフェース実装
	// @brief	IA + DrawIndexedInstancedを使って描画コマンドを積む
	void Draw(
		ID3D12GraphicsCommandList*	_pCmd,
		uint32_t					_isntanceCount = 1)override;

	bool		NeedsInputLayout()	const override { return true; }
	uint32_t	GetIndexCount()		const override { return m_IndexCount; }
	uint32_t	GetVertexCount()	const override { return m_VertexCount; }

private:

	// -------------------------------------------------------------------------------
	// @brief	バッファをUPLOADヒープに生成して初期データを書き込む
	// -------------------------------------------------------------------------------
	bool CreateBuffer(
		ID3D12Device*			_pDevice,
		size_t					_size,
		const void*				_pInitData,
		ComPtr<ID3D12Resource>& _outBuffer);

	// -------------------------------------------------------------------------------
	// private variables
	// -------------------------------------------------------------------------------
	ComPtr<ID3D12Resource>		m_pVB;					// 頂点バッファ
	ComPtr<ID3D12Resource>		m_pIB;					// インデックスバッファ
	D3D12_VERTEX_BUFFER_VIEW	m_VBV			= {};	// 頂点バッファビュー
	D3D12_INDEX_BUFFER_VIEW		m_IBV			= {};	// インデックスバッファビュー
	uint32_t					m_IndexCount	= 0;	// インデックス数
	uint32_t					m_VertexCount	= 0;	// 頂点数

	PolygonMeshResource	(const PolygonMeshResource&) = delete;
	void operator =		(const PolygonMeshResource&) = delete;

};




