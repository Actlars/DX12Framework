// -------------------------------------------------------------------------------
//		Includes
// -------------------------------------------------------------------------------
#include "TransientResourcePool.h"

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
RHI::TransientResourcePool::TransientResourcePool() 
{ /* DO_NOTHING */ }

RHI::TransientResourcePool::~TransientResourcePool()
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		初期化
// -------------------------------------------------------------------------------
bool RHI::TransientResourcePool::Init(ID3D12Device* _pDevice)
{
	m_pDevice = _pDevice;
	return true;
}

// -------------------------------------------------------------------------------
//		指定仕様のリソースを取得する（プールに再利用可能なものがあれば使用し、なければ新しく生成して返す）
// -------------------------------------------------------------------------------
ID3D12Resource* RHI::TransientResourcePool::Acquire(const Desc& _desc)
{
	// プールの中から使用が一致する未使用のものを探す
	for (auto& entry : m_Entries)
	{
		if (!entry.InUse &&
			entry.Desc.Width	== _desc.Width &&
			entry.Desc.Height	== _desc.Height &&
			entry.Desc.Format	== _desc.Format)
		{
			entry.InUse = true;
			return entry.pResource.Get();
		}
	}

	// 見つからなければ新規生成する
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension			= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Width				= _desc.Width;
	resDesc.Height				= _desc.Height;
	resDesc.DepthOrArraySize	= 1;
	resDesc.MipLevels			= 1;
	resDesc.Format				= _desc.Format;
	resDesc.SampleDesc.Count	= 1;
	resDesc.Flags				= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = _desc.Format;
	clearValue.Color[0] = _desc.ClearColor[0];
	clearValue.Color[1] = _desc.ClearColor[1];
	clearValue.Color[2] = _desc.ClearColor[2];
	clearValue.Color[3] = _desc.ClearColor[3];

	ComPtr<ID3D12Resource> pResource;
	auto hr = m_pDevice->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue,
		IID_PPV_ARGS(pResource.GetAddressOf()));
	if (FAILED(hr))
	{
		return nullptr;
	}

	Entry entry;
	entry.Desc		= _desc;
	entry.pResource = pResource;
	entry.InUse		= true;
	m_Entries.emplace_back(std::move(entry));

	return m_Entries.back().pResource.Get();
}

// -------------------------------------------------------------------------------
//		今フレーム使った仕様を未使用状態に戻す
// -------------------------------------------------------------------------------
void RHI::TransientResourcePool::ReleaseAll()
{
	for (auto& entry : m_Entries) 
	{ entry.InUse = false; }
}
