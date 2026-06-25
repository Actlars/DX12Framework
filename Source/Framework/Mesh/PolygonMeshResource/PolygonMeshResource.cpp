// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "PolygonMeshResource.h"
#include <Framework/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
//		初期化
// -------------------------------------------------------------------------------
bool PolygonMeshResource::Init(ID3D12Device* _pDevice, const ResMesh& _resMesh)
{
	if (_pDevice == nullptr) 
	{ return false; }

	if (_resMesh.Vertices.empty() || _resMesh.Indices.empty())
	{
		ELOG("PolygonMeshResource::Init() Empty mesh data");
		return false;
	}

	// 頂点バッファの生成
	const size_t vbSize = sizeof(ResMeshVertex) * _resMesh.Vertices.size();
	if (!CreateBuffer(_pDevice, vbSize, _resMesh.Vertices.data(), m_pVB))
	{
		ELOG("PolygonMeshResource::Init() VertexBuffer creation failed");
		return false;
	}

	// 頂点バッファビューの設定
	m_VBV.BufferLocation	= m_pVB->GetGPUVirtualAddress();
	m_VBV.SizeInBytes		= static_cast<UINT>(vbSize);
	m_VBV.StrideInBytes		= static_cast<UINT>(sizeof(ResMeshVertex));

	// インデックスバッファの生成
	const size_t ibSize = sizeof(uint32_t) * _resMesh.Indices.size();
	if (!CreateBuffer(_pDevice, ibSize, _resMesh.Indices.data(), m_pIB))
	{
		ELOG("PolygonMeshResource::Init() IndexBuffer creation failed");
		return false;
	}
	
	// インデックスバッファビューの設定
	m_IBV.BufferLocation	= m_pIB->GetGPUVirtualAddress();
	m_IBV.SizeInBytes		= static_cast<UINT>(ibSize);
	m_IBV.Format			= DXGI_FORMAT_R32_UINT;

	m_VertexCount	= static_cast<uint32_t>(_resMesh.Vertices.size());
	m_IndexCount	= static_cast<uint32_t>(_resMesh.Indices.size());

	return true;
}

// -------------------------------------------------------------------------------
//		終了処理
// -------------------------------------------------------------------------------
void PolygonMeshResource::Term()
{
	m_pVB.Reset();
	m_pIB.Reset();
	m_VBV			= {};
	m_IBV			= {};
	m_VertexCount	= 0;
	m_IndexCount	= 0;
}

// -------------------------------------------------------------------------------
//		描画コマンドを積む
// -------------------------------------------------------------------------------
void PolygonMeshResource::Draw(
	ID3D12GraphicsCommandList*	_pCmd,
	uint32_t					_instanceCount
)
{
	if (_pCmd == nullptr || m_IndexCount == 0) 
	{ return; }

	// IA（Input Assembler）にバッファをバインド
	_pCmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_pCmd->IASetVertexBuffers(0, 1, &m_VBV);
	_pCmd->IASetIndexBuffer(&m_IBV);

	// 描画命令を積む
	_pCmd->DrawIndexedInstanced(m_IndexCount, _instanceCount, 0, 0, 0);
}

// -------------------------------------------------------------------------------
//		バッファ生成ヘルパー（UPLOADヒープ）
// -------------------------------------------------------------------------------
bool PolygonMeshResource::CreateBuffer(
	ID3D12Device*			_pDevice,
	size_t					_size,
	const void*				_pInitData,
	ComPtr<ID3D12Resource>&	_outBuffer
)
{
	// UPLOADヒープに生成（CPUから直接書き込める）
	// 頻繁に更新しないスタティックメッシュは本来DEFAULTヒープが望ましいが
	// テクスチャと異なりメッシュはサイズが可変なので
	// UPLOADでも描画パフォーマンスへの影響が少ないためUPLOADを使用する
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
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_outBuffer.GetAddressOf()));
	if (FAILED(hr)) 
	{ return false; }

	// 初期データがある場合はMapして書き込む
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
