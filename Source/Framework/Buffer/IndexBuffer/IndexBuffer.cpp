// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "IndexBuffer.h"

// -------------------------------------------------------------------------------
// IndexBuffer class
// -------------------------------------------------------------------------------

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
IndexBuffer::IndexBuffer()
	: m_pIB(nullptr) 
{ memset(&m_View, 0, sizeof(m_View)); }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
IndexBuffer::~IndexBuffer() 
{ Term(); }

// -------------------------------------------------------------------------------
//		初期化処理を行う
// -------------------------------------------------------------------------------
bool IndexBuffer::Init(ID3D12Device* _pDevice, size_t _size, const uint32_t* _pInitData)
{
	// ヒーププロパティ
	D3D12_HEAP_PROPERTIES prop = {};
	prop.Type					= D3D12_HEAP_TYPE_UPLOAD;
	prop.CPUPageProperty		= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	prop.MemoryPoolPreference	= D3D12_MEMORY_POOL_UNKNOWN;
	prop.CreationNodeMask		= 1;
	prop.VisibleNodeMask		= 1;

	// リソースの設定
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment			= 0;
	desc.Width				= UINT64(_size);
	desc.Height				= 1;
	desc.DepthOrArraySize	= 1;
	desc.MipLevels			= 1;
	desc.Format				= DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count	= 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout				= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags				= D3D12_RESOURCE_FLAG_NONE;

	// リソースを生成
	auto hr = _pDevice->CreateCommittedResource(
		&prop,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_pIB.GetAddressOf()));
	if (FAILED(hr)) 
	{ return false; }

	// インデックスバッファビューの設定
	m_View.BufferLocation	= m_pIB->GetGPUVirtualAddress();
	m_View.Format			= DXGI_FORMAT_R32_UINT;
	m_View.SizeInBytes		= UINT(_size);

	// 初期化データがあれば、書き込んでおく
	if (_pInitData != nullptr)
	{
		void* ptr = Map();
		if (ptr == nullptr) 
		{ return false; }

		memcpy(ptr, _pInitData, _size);

		m_pIB->Unmap(0, nullptr);
	}

	// 正常終了
	return true;
}

// -------------------------------------------------------------------------------
//		終了処理を行う
// -------------------------------------------------------------------------------
void IndexBuffer::Term()
{
	m_pIB.Reset();
	memset(&m_View, 0, sizeof(m_View));
}

// -------------------------------------------------------------------------------
//		メモリマッピングを行う
// -------------------------------------------------------------------------------
uint32_t* IndexBuffer::Map()
{
	uint32_t* ptr;
	auto hr = m_pIB->Map(0, nullptr, reinterpret_cast<void**>(&ptr));
	if (FAILED(hr)) 
	{ return nullptr; }

	return ptr;
}

// -------------------------------------------------------------------------------
//		メモリマッピングを解除
// -------------------------------------------------------------------------------
void IndexBuffer::Unmap() 
{ m_pIB->Unmap(0, nullptr); }

// -------------------------------------------------------------------------------
//		インデックスバッファビューを取得
// -------------------------------------------------------------------------------
D3D12_INDEX_BUFFER_VIEW IndexBuffer::GetView() const 
{ return m_View; }
