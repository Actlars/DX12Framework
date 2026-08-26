// -------------------------------------------------------------------------------
//		Includes
// -------------------------------------------------------------------------------
#include "Buffer.h"
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
//		初期化
// -------------------------------------------------------------------------------
bool RHI::Buffer::Init(ID3D12Device* _pDevice, const BufferDesc _desc, const void* _pInitData)
{
	if (_pDevice == nullptr || _desc.SizeInBytes == 0)
	{
		ELOG("Buffer::Init() invalid argument");
		return false;
	}

	D3D12_HEAP_PROPERTIES	heapProp = {};
	D3D12_RESOURCE_STATES	initialState = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_FLAGS	resFlags = D3D12_RESOURCE_FLAG_NONE;

	switch (_desc.HeapType)
	{
	case BufferHeapType::Upload:
		heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
		// UPLOADヒープのリソースはGENERIC_READ状態のまま固定という制約がある
		// ここから他の状態に遷移することはできない
		initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
		break;
	case BufferHeapType::Default:
		heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

		// -------------------------------------------------------------------------------
		// バッファは必ずCOMMONで作られる
		//
		// UNORDERED_ACCESS等を指定しても D3D12 側で無視され、
		// デバッグレイヤーに「Ignoring InitialState」の情報メッセージが出るだけになる
		// COMMONからは最初の使用時に自動で昇格するため、これで正しく動く
		// -------------------------------------------------------------------------------
		initialState = D3D12_RESOURCE_STATE_COMMON;

		if (_desc.AllowUAV)
		{
			// UAVとして使うことをリソース生成時に明示する必要がある
			// 後から付け足すことはできないフラグ
			resFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}
		break;
	case BufferHeapType::Readback:
		heapProp.Type = D3D12_HEAP_TYPE_READBACK;
		// READBACKヒープのリソースはCOPY_DEST状態のまま固定という制約がある
		initialState = D3D12_RESOURCE_STATE_COPY_DEST;
		break;
	}

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width				= static_cast<UINT64>(_desc.SizeInBytes);
	desc.Height				= 1;
	desc.DepthOrArraySize	= 1;
	desc.MipLevels			= 1;
	desc.Format				= DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count	= 1;
	desc.Layout				= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags				= resFlags;

	auto hr = _pDevice->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE, &desc,
		initialState, nullptr, IID_PPV_ARGS(m_pResource.GetAddressOf()));

	if (FAILED(hr))
	{
		ELOG("Buffer::Init() : CreateCommittedResource failed hr = 0x%08X", hr);
		return false;
	}

	m_Size = _desc.SizeInBytes;
	m_HeapType = _desc.HeapType;

	// Upload/Readbackは常時マップしておく
	if (_desc.HeapType == BufferHeapType::Upload)
	{
		// UPLOADヒープは「CPU→GPU書き込み専用」として扱う想定なので
		// 読み取り範囲なし({0,0})を渡してMapする
		D3D12_RANGE readRange = { 0,0 };
		hr = m_pResource->Map(0, &readRange, &m_pMappedPtr);
	}
	else if (_desc.HeapType == BufferHeapType::Readback)
	{
		// Readbackヒープは「GPU→CPU読み取り専用」として使う想定なので
		// 書き込み範囲なし（nullptr = 全領域読み取り可能）でMapする
		hr = m_pResource->Map(0, nullptr, &m_pMappedPtr);
	}

	if (FAILED(hr))
	{
		ELOG("Buffer::Init() : Map failed hr = 0x%08X", hr);
		return false;
	}

	// 初期データがあれば、UPLOADヒープの場合のみここでCPU→GPUコピーする
	// (UPLOADヒープはマップ済みの仮想アドレスに書き込むだけでGPUに転送される)
	if (_pInitData != nullptr && _desc.HeapType == BufferHeapType::Upload)
	{
		memcpy(m_pMappedPtr, _pInitData, _desc.SizeInBytes);
	}

	return true;
}

// -------------------------------------------------------------------------------
//		終了
// -------------------------------------------------------------------------------
void RHI::Buffer::Term()
{
	if (m_pMappedPtr != nullptr && m_pResource != nullptr)
	{
		m_pResource->Unmap(0, nullptr);
		m_pMappedPtr = nullptr;
	}
	m_pResource.Reset();
	m_Size = 0;
}

// -------------------------------------------------------------------------------
//		CPUから書き込み
// -------------------------------------------------------------------------------
void RHI::Buffer::Write(const void* _pData, size_t _size, size_t _offset)
{
	if (m_pMappedPtr == nullptr)
	{
		ELOG("Buffer::Write() : this buffer is not CPU-writable (HeapType::Default?)");
		return;
	}

	memcpy(static_cast<uint8_t*>(m_pMappedPtr) + _offset, _pData, _size);
}

// -------------------------------------------------------------------------------
//		CPUから読み取り
// -------------------------------------------------------------------------------
void RHI::Buffer::Read(void* _pOutData, size_t _size, size_t _offset) const
{
	if (m_pMappedPtr == nullptr)
	{
		ELOG("Buffer::Read() : this buffer is not CPU-readable (HeapType::Default or Upload?)");
		return;
	}
	memcpy(_pOutData, static_cast<uint8_t*>(m_pMappedPtr) + _offset, _size);
}

// -------------------------------------------------------------------------------
//		GPU仮想アドレスの取得
// -------------------------------------------------------------------------------
D3D12_GPU_VIRTUAL_ADDRESS RHI::Buffer::GetAddress() const
{
	return m_pResource != nullptr ? m_pResource->GetGPUVirtualAddress() : 0;
}
