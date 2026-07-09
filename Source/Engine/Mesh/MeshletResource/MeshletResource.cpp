// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "MeshletResource.h"
#include <Engine/Utility/Debug/Logger/Logger.h>

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
//		終了処理
// -------------------------------------------------------------------------------
void MeshletResource::Term()
{
	m_pVertexBuffer.Reset();
	m_pIndexBuffer.Reset();
	m_VertexCount	= 0;
	m_IndexCount	= 0;
	m_VerticesSlot	= UINT32_MAX;
	m_IndicesSlot	= UINT32_MAX;
}

// -------------------------------------------------------------------------------
//		RootDescriptorスロット番号の設定
// -------------------------------------------------------------------------------
void MeshletResource::SetRootSlots(uint32_t _verticesSlot, uint32_t _indicesSlot)
{
	m_VerticesSlot = _verticesSlot;
	m_IndicesSlot = _indicesSlot;
}

// -------------------------------------------------------------------------------
//		描画コマンドを積む
// -------------------------------------------------------------------------------
void MeshletResource::Draw(
	ID3D12GraphicsCommandList* _pCmd,
	uint32_t					_instanceCount
)
{
	if (_pCmd == nullptr || m_IndexCount == 0)
	{
		return;
	}

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

	// DispatchMeshはID3D12GraphicsCommandList6以降のメソッドなのでQueryInterfaceする
	ComPtr<ID3D12GraphicsCommandList6> pCmd6;
	auto hr = _pCmd->QueryInterface(IID_PPV_ARGS(pCmd6.GetAddressOf()));
	if (FAILED(hr))
	{
		ELOG("MeshletResource::Draw() : QueryInterface(ID3D12GraphicsCommandList6) failed hr = 0x%08X", hr);
		return;
	}

	// 現状は三角形数分をまとめて1スレッドグループで処理する最小構成
	// (numthreads(64,1,1)なので、1グループで最大21三角形程度まで対応可能
	// 大規模メッシュではメッシュレットで分割してスレッドグループ数を増やす必要がある
	pCmd6->DispatchMesh(_instanceCount, 1, 1);
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
