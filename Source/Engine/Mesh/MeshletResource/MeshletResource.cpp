// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "MeshletResource.h"
#include <Engine/Utility/Debug/Logger/Logger.h>
#include "meshoptimizer.h"

// -------------------------------------------------------------------------------
//		コンストラクタ	
// -------------------------------------------------------------------------------
MeshletResource::MeshletResource()
{ /* DO_NOTHING */
}

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
MeshletResource::~MeshletResource()
{
	Term();
}

// -------------------------------------------------------------------------------
//		初期化
// -------------------------------------------------------------------------------
bool MeshletResource::Init(ID3D12Device* _pDevice, const ResMesh& _resMesh)
{
	MeshletVertexData vertices;
	vertices.pData = _resMesh.Vertices.data();
	vertices.Stride = sizeof(ResMeshVertex);
	vertices.Count = static_cast<uint32_t>(_resMesh.Vertices.size());

	MeshletIndexData indices;
	indices.pData = _resMesh.Indices.data();
	indices.Count = static_cast<uint32_t>(_resMesh.Indices.size());

	return Init(_pDevice, vertices, indices);
}

// -------------------------------------------------------------------------------
//		初期化（ResMesh用の薄いラッパー）
// -------------------------------------------------------------------------------

bool MeshletResource::Init(ID3D12Device* _pDevice, const MeshletVertexData& _vertices, const MeshletIndexData& _indices)
{
	if (_pDevice == nullptr || _vertices.pData == nullptr || _indices.pData == nullptr)
	{
		ELOG("MeshletResource::Init() Invalid argument");
		return false;
	}

	if (_vertices.Count == 0 || _indices.Count == 0)
	{
		ELOG("MeshletResource::Init() Empty mesh data");
		return false;
	}

	if (_indices.Count % 3 != 0)
	{
		ELOG("MeshletResource::Init() indexCount is not a multiple of 3");
		return false;
	}

	const size_t vbSize = _vertices.Stride * _vertices.Count;
	if (!CreateStructuredBuffer(_pDevice, vbSize, _vertices.pData, m_pVertexBuffer))
	{
		ELOG("MeshletResource::Init() VertexBuffer creation failed");
		return false;
	}

	const size_t ibSize = sizeof(uint32_t) * _indices.Count;
	if (!CreateStructuredBuffer(_pDevice, ibSize, _indices.pData, m_pIndexBuffer))
	{
		ELOG("MeshletResource::Init() IndexBuffer creation failed");
		return false;
	}

	m_VertexCount = _vertices.Count;
	m_IndexCount = _indices.Count;

	return true;
}

// -------------------------------------------------------------------------------
//		メッシュレットの初期化
// -------------------------------------------------------------------------------
bool MeshletResource::InitMeshlets(ID3D12Device* _pDevice, const ResMesh& _resMesh)
{
	if (_pDevice == nullptr || _resMesh.Vertices.empty() || _resMesh.Indices.empty()) 
	{ 
		ELOG("MeshletResource::InitMeshlets() Invalid argument or empty mesh");
		return false; 
	}

	// meshoptimizerのパラメータ
	// max_vertices / max_trianglesはハードウェアの制約に合わせる
	// SM6.9世代のGPUだと 64頂点 / 124三角形 が推奨値
	const size_t maxVertices = 64;
	const size_t maxTriangles = 124;
	const float coneWeight = 0.0f;	// 今回はカリング用コーンは使わない

	const size_t maxMeshlets = meshopt_buildMeshletsBound(
		_resMesh.Indices.size(), maxVertices, maxTriangles);

	std::vector<meshopt_Meshlet>	meshlets(maxMeshlets);
	std::vector<unsigned int>		meshletVertices(maxMeshlets * maxVertices);
	std::vector<unsigned char>		meshletTriangles(maxMeshlets * maxTriangles * 3);

	const size_t meshletCount = meshopt_buildMeshlets(
		meshlets.data(),
		meshletVertices.data(),
		meshletTriangles.data(),
		_resMesh.Indices.data(),
		_resMesh.Indices.size(),
		&_resMesh.Vertices[0].Position.x,	// 頂点座標の先頭ポインタ
		_resMesh.Vertices.size(),
		sizeof(ResMeshVertex),				// 頂点スライド
		maxVertices, maxTriangles, coneWeight);

	// meshopt_buildMeshletsは上限確保した配列を返すので、実際に使われた分だけに切り詰める
	const auto& last = meshlets[meshletCount - 1];
	meshletVertices.reserve(last.vertex_offset + last.vertex_count);
	meshletTriangles.reserve(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
	meshlets.reserve(meshletCount);

	// PrimitiveIndicesを3バイト→1uintへバック
	// GPU側は1三角形 = 1uintとして読む（下位24bitに3頂点分のローカルインデックスを詰める）
	std::vector<MeshletDesc>	descs(meshletCount);
	std::vector<uint32_t>		packedPrimitives;
	packedPrimitives.reserve(meshletTriangles.size() / 3);
	
	for (size_t i = 0; i < meshletCount; ++i)
	{
		const auto& m = meshlets[i];

		descs[i].VertexOffset		= m.vertex_offset;
		descs[i].VertexCount		= m.vertex_count;
		descs[i].PrimitiveOffset	= static_cast<uint32_t>(packedPrimitives.size());
		descs[i].PrimitiveCount		= m.triangle_count;

		for (uint32_t t = 0; t < m.triangle_count; ++t)
		{
			const uint8_t* tri = &meshletTriangles[m.triangle_offset + t * 3];
			// 下位8bitずつ3頂点のローカルインデックスを詰める
			const uint32_t packed = 
				(static_cast<uint32_t>(tri[0])) |
				(static_cast<uint32_t>(tri[1]) << 8) |
				(static_cast<uint32_t>(tri[2]) << 16);
			packedPrimitives.emplace_back(packed);
		}
	}

	// GPUバッファへ転送
	if (!CreateStructuredBuffer(_pDevice,
		sizeof(unsigned int) * meshletVertices.size(), meshletVertices.data(), m_pMeshletVertexIndices))
	{
		ELOG("MeshletResource::InitMeshlets() MeshletVertexIndices creation failed");
		return false;
	}

	if (!CreateStructuredBuffer(_pDevice,
		sizeof(uint32_t) * packedPrimitives.size(), packedPrimitives.data(), m_pPackedPrimitiveIndices))
	{
		ELOG("MeshletResource::InitMeshlets() PackedPrimitives creation failed");
		return false;
	}

	if (!CreateStructuredBuffer(_pDevice,
		sizeof(MeshletDesc) * descs.size(), descs.data(), m_pMeshlets))
	{
		ELOG("MeshletResource::InitMeshlets() Meshlets creation failed");
		return false;
	}

	if (!CreateStructuredBuffer(_pDevice,
		sizeof(ResMeshVertex) * _resMesh.Vertices.size(), _resMesh.Vertices.data(), m_pVertexBuffer))
	{
		ELOG("MeshletResource::InitMeshlets() VertexBuffer creation failed");
		return false;
	}

	m_VertexCount	= static_cast<uint32_t>(_resMesh.Vertices.size());
	m_MeshletCount = static_cast<uint32_t>(meshletCount);

	for (size_t i = 0; i < std::min<size_t>(3, descs.size()); ++i)
	{
		/*ELOG("Meshlet[%zu]: VertexOffset=%u VertexCount=%u PrimitiveOffset=%u PrimitiveCount=%u",
			i, descs[i].VertexOffset, descs[i].VertexCount, descs[i].PrimitiveOffset, descs[i].PrimitiveCount);*/
	}

	/*ELOG("meshletVertices.size()=%zu packedPrimitives.size()=%zu meshlets.size()=%zu",
		meshletVertices.size(), packedPrimitives.size(), descs.size());*/

	return true;
}

// -------------------------------------------------------------------------------
//		終了処理
// -------------------------------------------------------------------------------
void MeshletResource::Term()
{
	m_pVertexBuffer.Reset();
	m_pIndexBuffer.Reset();
	m_pMeshletVertexIndices.Reset();
	m_pPackedPrimitiveIndices.Reset();
	m_pMeshlets.Reset();

	m_VertexCount	= 0;
	m_IndexCount	= 0;
	m_MeshletCount	= 0;

	m_VerticesSlot			= UINT32_MAX;
	m_IndicesSlot			= UINT32_MAX;
	m_MeshletVerticesSlot	= UINT32_MAX;
	m_PrimitiveIndicesSlot	= UINT32_MAX;
	m_MeshletsSlot			= UINT32_MAX;
}

// -------------------------------------------------------------------------------
//		RootDescriptorスロット番号の設定
// -------------------------------------------------------------------------------
void MeshletResource::SetRootSlots(uint32_t _verticesSlot, uint32_t _indicesSlot)
{
	m_VerticesSlot = _verticesSlot;
	m_IndicesSlot = _indicesSlot;
}

void MeshletResource::SetMeshletRootSlots(
	uint32_t _verticesSlot, 
	uint32_t _meshletVerticesSlot, 
	uint32_t _primitiveIndicesSlot, 
	uint32_t _meshletsSlot)
{
	m_VerticesSlot			= _verticesSlot;
	m_MeshletVerticesSlot	= _meshletVerticesSlot;
	m_PrimitiveIndicesSlot	= _primitiveIndicesSlot;
	m_MeshletsSlot			= _meshletsSlot;
}

// -------------------------------------------------------------------------------
//		描画コマンドを積む
// -------------------------------------------------------------------------------
void MeshletResource::Draw(
	ID3D12GraphicsCommandList*	_pCmd,
	uint32_t					_instanceCount
)
{
	if (_pCmd == nullptr) 
	{ return; }

	ComPtr<ID3D12GraphicsCommandList6> pCmd6;
	if (FAILED(_pCmd->QueryInterface(IID_PPV_ARGS(pCmd6.GetAddressOf())))) 
	{ return; }

	if (m_MeshletCount > 0)
	{
		// 実モデル描画パス
		if (m_VerticesSlot			!= UINT32_MAX) 
		{ _pCmd->SetGraphicsRootShaderResourceView(m_VerticesSlot, m_pVertexBuffer->GetGPUVirtualAddress()); }
		if (m_MeshletVerticesSlot	!= UINT32_MAX)
		{ _pCmd->SetGraphicsRootShaderResourceView(m_MeshletVerticesSlot, m_pMeshletVertexIndices->GetGPUVirtualAddress()); }
		if (m_PrimitiveIndicesSlot	!= UINT32_MAX)
		{ _pCmd->SetGraphicsRootShaderResourceView(m_PrimitiveIndicesSlot, m_pPackedPrimitiveIndices->GetGPUVirtualAddress()); }
		if (m_MeshletsSlot			!= UINT32_MAX) 
		{ _pCmd->SetGraphicsRootShaderResourceView(m_MeshletsSlot, m_pMeshlets->GetGPUVirtualAddress()); }

		pCmd6->DispatchMesh(m_MeshletCount, 1, 1);	// メッシュレット数分スレッドグループを回す
	}
	else
	{
		// Vertices / Indices を Root Descriptor(SRV)にバインド
		// スロットが未設定の場合はバインドをスキップして無効呼び出しを防ぐ
		if (m_VerticesSlot != UINT32_MAX)
		{
			_pCmd->SetGraphicsRootShaderResourceView(m_VerticesSlot, m_pVertexBuffer->GetGPUVirtualAddress());
		}

		if (m_IndicesSlot != UINT32_MAX)
		{
			_pCmd->SetGraphicsRootShaderResourceView(m_IndicesSlot, m_pIndexBuffer->GetGPUVirtualAddress());
		}
		// 現状は三角形数分をまとめて1スレッドグループで処理する最小構成
		// (numthreads(64,1,1)なので、1グループで最大21三角形程度まで対応可能
		// 大規模メッシュではメッシュレットで分割してスレッドグループ数を増やす必要がある
		pCmd6->DispatchMesh(_instanceCount, 1, 1);
	}
}

bool MeshletResource::CreateStructuredBuffer(
	ID3D12Device* _pDevice, 
	size_t _size,
	const void* _pInitData, 
	ComPtr<ID3D12Resource>& _outBuffer)
{
	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width				= static_cast<UINT64>(_size);
	desc.Height				= 1;
	desc.DepthOrArraySize	= 1;
	desc.MipLevels			= 1;
	desc.Format				= DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count	= 1;
	desc.Layout				= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags				= D3D12_RESOURCE_FLAG_NONE;

	auto hr = _pDevice->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(_outBuffer.GetAddressOf()));
	if (FAILED(hr)) 
	{ return false; }
	
	if (_pInitData != nullptr)
	{
		void* ptr = nullptr;
		hr = _outBuffer->Map(0, nullptr, &ptr);
		if (FAILED(hr)) 
		{ return false; }
		memcpy(ptr, _pInitData, _size);
		_outBuffer->Unmap(0, nullptr);
	}

	return true;
}
