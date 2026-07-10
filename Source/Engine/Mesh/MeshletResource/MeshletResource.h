#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "../MeshResource.h"
#include "../ResData.h"

// -------------------------------------------------------------------------------
// MeshletResource class
// 
// 概要 : 
//	MeshShaderパイプライン用のMeshResource実装
//	頂点・インデックスデータをStructuredBufferとしてUPLOADヒープに生成し
//	RootDescriptor（SRV）としてシェーダーへ直接バインドする
// 
//	PolygonMeshResourceとは異なり、VB/IBビュー・IAは使用しない
//	Draw内で、SetGraphicsRootShaderResourceView + DispatchMeshを呼ぶ
// -------------------------------------------------------------------------------
class MeshletResource final : public MeshResource
{
public:

	// -------------------------------------------------------------------------------
	// MeshletVertexData structure
	// 
	// 概要 : 
	//	頂点データの受け渡し用。頂点構造体のレイアウトは問わない
	// -------------------------------------------------------------------------------
	struct MeshletVertexData
	{
		const void* pData = nullptr;	// 頂点データの先頭ポインタ
		size_t		Stride = 0;			// 頂点1個分のバイトサイズ
		uint32_t	Count = 0;			// 頂点数
	};

	// -------------------------------------------------------------------------------
	// GPUメッシュレット記述子
	// -------------------------------------------------------------------------------
	struct MeshletDesc
	{
		uint32_t VertexOffset;		// MeshletVerticesバッファ内の開始位置
		uint32_t VertexCount;		// このメッシュレットの頂点数（最大64）
		uint32_t PrimitiveOffset;	// PackedPrimitiveIndicesバッファ内の開始位置（uint単位）
		uint32_t PrimitiveCount;	// このメッシュレットの三角形数（最大124程度）
	};

	// -------------------------------------------------------------------------------
	// MeshletIndexData structure
	// 
	// 概要 : 
	//	インデックスデータの受け渡し用（uint32_t、3個で三角形1つ）
	// -------------------------------------------------------------------------------
	struct MeshletIndexData
	{
		const uint32_t* pData = nullptr;	// インデックスデータの先頭ポインタ
		uint32_t		Count = 0;			// インデックス数（3の倍数であること）
	};


	// -------------------------------------------------------------------------------
	// コンストラクタ
	// -------------------------------------------------------------------------------
	MeshletResource();

	// -------------------------------------------------------------------------------
	// デストラクタ
	// -------------------------------------------------------------------------------
	~MeshletResource();

	// -------------------------------------------------------------------------------
	// @brief	初期化（ResMeshからGPUリソースを生成）
	// -------------------------------------------------------------------------------
	bool Init(ID3D12Device* _pDevice, const ResMesh& _resMesh);

	// -------------------------------------------------------------------------------
	// @brief	初期化（任意の頂点データ + インデックスデータからGPUリソースを生成）
	// 
	// @param[in]	_pDevice	デバイス
	// @param[in]	_vertices	頂点データ
	// @param[in]	_indices	インデックスデータ
	// @retval	true	成功
	// @retval	false	失敗
	// -------------------------------------------------------------------------------
	bool Init(
		ID3D12Device*				_pDevice,
		const MeshletVertexData&	_vertices,
		const MeshletIndexData&		_indices);

	// -------------------------------------------------------------------------------
	// @brief	meshoptimizerでメッシュレット分割を行い、GPUリソースを生成する
	//			実モデル描画用のパス
	// 
	// @param[in]	_pDevice	デバイス
	// @param[in]	_resMesh	ロード済みの生メッシュデータ
	// @retval	true	成功
	// @retval	false	失敗
	// -------------------------------------------------------------------------------
	bool InitMeshlets(ID3D12Device* _pDevice, const ResMesh& _resMesh);

	// -------------------------------------------------------------------------------
	// @brief	終了処理
	// -------------------------------------------------------------------------------
	void Term();

	// -------------------------------------------------------------------------------
	// @brief	Root Descriptr(SRV)としてバインドするスロット番号を設定
	//			RootSignatureLayout::GetSlotで取得した値を渡す
	// 
	// @param[in]	_verticesSlot	Vertices(StructuredBuffer)のスロット番号
	// @param[in]	_indicesSlot	Indices(StructuredBuffer)のスロット番号
	// -------------------------------------------------------------------------------
	void SetRootSlots(uint32_t _verticesSlot, uint32_t _indicesSlot);

	// -------------------------------------------------------------------------------
	// @brief	実モデル用のRoot Descriptor(SRV)スロットを設定
	// -------------------------------------------------------------------------------
	void SetMeshletRootSlots(
		uint32_t _verticesSlot,
		uint32_t _meshletVerticesSlot,
		uint32_t _primitiveIndicesSlot,
		uint32_t _meshletsSlot);

	// MeshResourceインターフェース実装
	// @brief	Vertices/IndicesをRootSRVとしてバインドし、DispatchMeshで描画コマンドを積む
	void Draw(
		ID3D12GraphicsCommandList*	_pCmd,
		uint32_t					_instanceCount = 1)override;

	bool		NeedsInputLayout()	const override { return true; }
	uint32_t	GetIndexCount()		const override { return m_IndexCount; }
	uint32_t	GetVertexCount()	const override { return m_VertexCount; }

	// -------------------------------------------------------------------------------
	// @brief	三角形数を返す（メッシュレット / スレッドグループ数の算出に使う）
	// -------------------------------------------------------------------------------
	uint32_t GetTriangleCount() const { return m_IndexCount / 3; }

	// -------------------------------------------------------------------------------
	// @brief	メッシュレット数を返す（DispatchMeshの引数に使う）
	// -------------------------------------------------------------------------------
	uint32_t GetMeshletCount() const { return m_MeshletCount; }

private:

	// -------------------------------------------------------------------------------
	// @brief	StructuredBuffer用のUPLOADヒープバッファを生成する
	// -------------------------------------------------------------------------------
	bool CreateStructuredBuffer(
		ID3D12Device* _pDevice,
		size_t _size,
		const void* _pInitData,
		ComPtr<ID3D12Resource>& _outBuffer);

	// -------------------------------------------------------------------------------
	// private variables
	// -------------------------------------------------------------------------------
	ComPtr<ID3D12Resource>		m_pVertexBuffer;		// 頂点StructuredBuffer
	ComPtr<ID3D12Resource>		m_pIndexBuffer;			// インデックスStructuredBuffer（uint3単位）
	uint32_t					m_VertexCount	= 0;	// 頂点数
	uint32_t					m_IndexCount	= 0;	// インデックス数（3の倍数、三角形数 * 3）

	uint32_t					m_VerticesSlot	= UINT32_MAX;	// RootSignatureLayout上のVerticesスロット
	uint32_t					m_IndicesSlot	= UINT32_MAX;	// RootSignatureLayout上のIndicesスロット

	ComPtr<ID3D12Resource>		m_pMeshletVertexIndices;	// ローカル→グローバル頂点インデックス変換テーブル
	ComPtr<ID3D12Resource>		m_pPackedPrimitiveIndices;	// 3バイトパック済み三角形インデックス
	ComPtr<ID3D12Resource>		m_pMeshlets;				// MeshletDesc配列
	uint32_t					m_MeshletCount = 0;

	uint32_t m_MeshletVerticesSlot	= UINT32_MAX;
	uint32_t m_PrimitiveIndicesSlot = UINT32_MAX;
	uint32_t m_MeshletsSlot			= UINT32_MAX;

	MeshletResource		(const MeshletResource&) = delete;
	void operator =		(const MeshletResource&) = delete;

};




