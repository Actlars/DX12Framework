// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "DepthTarget.h"
#include <Engine/Pool/DescriptorPool/DescriptorPool.h>

// -------------------------------------------------------------------------------
// DepthTarget class
// -------------------------------------------------------------------------------

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
DepthTarget::DepthTarget()
: m_pTarget		(nullptr)
, m_pHandleDSV	(nullptr)
, m_pPoolDSV	(nullptr)
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
DepthTarget::~DepthTarget() 
{ Term(); }

// -------------------------------------------------------------------------------
//		初期化処理
// -------------------------------------------------------------------------------
bool DepthTarget::Init
(
	ID3D12Device*	_pDevice,
	DescriptorPool* _pPoolRTV,
	uint32_t		_width,
	uint32_t		_height,
	DXGI_FORMAT		_format
)
{
	if (_pDevice == nullptr || _pPoolRTV == nullptr || _width == 0 || _height == 0) 
	{ return false; }

	assert(m_pHandleDSV == nullptr);
	assert(m_pPoolDSV	== nullptr);

	m_pPoolDSV = _pPoolRTV;
	m_pPoolDSV->AddRef();

	m_pHandleDSV = m_pPoolDSV->AllocHandle();
	if (m_pHandleDSV == nullptr) 
	{ return false; }

	D3D12_HEAP_PROPERTIES prop = {};
	prop.Type					= D3D12_HEAP_TYPE_DEFAULT;
	prop.CPUPageProperty		= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	prop.MemoryPoolPreference	= D3D12_MEMORY_POOL_UNKNOWN;
	prop.CreationNodeMask		= 1;
	prop.VisibleNodeMask		= 1;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension			= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment			= 0;
	desc.Width				= UINT64(_width);
	desc.Height				= _height;
	desc.DepthOrArraySize	= 1;
	desc.MipLevels			= 1;
	desc.Format				= _format;
	desc.SampleDesc.Count	= 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout				= D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags				= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearValue;
	clearValue.Format				= _format;
	clearValue.DepthStencil.Depth	= 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	auto hr = _pDevice->CreateCommittedResource(
		&prop,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(m_pTarget.GetAddressOf()));
	if (FAILED(hr)) 
	{ return false; }

	m_ViewDesc.ViewDimension		= D3D12_DSV_DIMENSION_TEXTURE2D;
	m_ViewDesc.Format				= _format;
	m_ViewDesc.Texture2D.MipSlice	= 0;
	m_ViewDesc.Flags				= D3D12_DSV_FLAG_NONE;

	_pDevice->CreateDepthStencilView(
		m_pTarget.Get(),
		&m_ViewDesc,
		m_pHandleDSV->HandleCPU);

	return true;
}

// -------------------------------------------------------------------------------
//		終了処理
// -------------------------------------------------------------------------------
void DepthTarget::Term()
{
	m_pTarget.Reset();

	if (m_pPoolDSV != nullptr && m_pHandleDSV != nullptr)
	{
		m_pPoolDSV->FreeHandle(m_pHandleDSV);
		m_pHandleDSV = nullptr;
	}

	if (m_pPoolDSV != nullptr)
	{
		m_pPoolDSV->Release();
		m_pPoolDSV = nullptr;
	}
}

// -------------------------------------------------------------------------------
//		ディスクリプタハンドル（DSV用）を取得
// -------------------------------------------------------------------------------
DescriptorHandle* DepthTarget::GetHandleDSV() const 
{ return m_pHandleDSV; }

// -------------------------------------------------------------------------------
//		リソースを取得
// -------------------------------------------------------------------------------
ID3D12Resource* DepthTarget::GetResource() const 
{ return m_pTarget.Get(); }

// -------------------------------------------------------------------------------
//		リソース設定を取得
// -------------------------------------------------------------------------------
D3D12_RESOURCE_DESC DepthTarget::GetDesc() const
{
	if(m_pTarget == nullptr) 
	{ return D3D12_RESOURCE_DESC(); }

	return m_pTarget->GetDesc();
}

// -------------------------------------------------------------------------------
//		深度ステンシルビューの設定を取得
// -------------------------------------------------------------------------------
D3D12_DEPTH_STENCIL_VIEW_DESC DepthTarget::GetViewDesc() const 
{ return m_ViewDesc; }
