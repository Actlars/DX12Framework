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
	// テスト用: 三角形1つ分の頂点・インデックスデータ
	// -------------------------------------------------------------------------------
	struct TriangleVertex
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT4 Color;
	};

	// 頂点データ（MSInputと1対1で対応させる）
	const TriangleVertex kVertices[3] =
	{
		{ {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
	};

	// インデックスデータ（uint3を1個 = 三角形1個ぶん）
	const uint32_t kIndices[3] = { 0, 1, 2 };


	// -------------------------------------------------------------------------------
	// MeshletVertexData structure
	// 
	// 概要 : 
	//	頂点データの受け渡し用。頂点構造体のレイアウトは問わない
	// -------------------------------------------------------------------------------
	struct MeshletVertexData
	{
		const void* pData = nullptr;	// 頂点データの先頭ポインタ
		size_t		Stride = 0;		// 頂点1個分のバイトサイズ
		uint32_t	Count = 0;		// 頂点数
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
		uint32_t			Count = 0;		// インデックス数（3の倍数であること）
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
		ID3D12Device* _pDevice,
		const MeshletVertexData& _vertices,
		const MeshletIndexData& _indices);

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
	ComPtr<ID3D12Resource>		m_pVertexBuffer;	// 頂点StructuredBuffer
	ComPtr<ID3D12Resource>		m_pIndexBuffer;		// インデックスStructuredBuffer（uint3単位）
	uint32_t					m_VertexCount = 0;	// 頂点数
	uint32_t					m_IndexCount = 0;	// インデックス数（3の倍数、三角形数 * 3）

	uint32_t					m_VerticesSlot	= UINT32_MAX;	// RootSignatureLayout上のVerticesスロット
	uint32_t					m_IndicesSlot	= UINT32_MAX;	// RootSignatureLayout上のIndicesスロット

	MeshletResource		(const MeshletResource&) = delete;
	void operator =		(const MeshletResource&) = delete;

};




