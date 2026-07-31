// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "RGResourceRegistry.h"
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
//		外部で生成済みのリソースをグラフに取り込む
// -------------------------------------------------------------------------------
RG::Handle RG::ResourceRegistry::Import(const std::string& _name, ID3D12Resource* _pResource)
{
	RG::Handle h = static_cast<RG::Handle>(m_Resources.size());
	Entry entry;
	entry.name		= _name;
	entry.pResource = _pResource;
	m_Resources.push_back(entry);
	return h;
}

RG::Handle RG::ResourceRegistry::CreateTransient(
	const std::string&				_name, 
	const TransientResourceDesc&	_desc, 
	RHI::TransientResourcePool*		_pResourcePool)
{
	RHI::TransientResourcePool::Desc poolDesc;
	poolDesc.Width			= _desc.Width;
	poolDesc.Height			= _desc.Height;
	poolDesc.Format			= _desc.Format;
	poolDesc.ClearColor[0]	= _desc.ClearColor[0];
	poolDesc.ClearColor[1]	= _desc.ClearColor[1];
	poolDesc.ClearColor[2]	= _desc.ClearColor[2];
	poolDesc.ClearColor[3]	= _desc.ClearColor[3];

	auto* pResource = _pResourcePool->Acquire(poolDesc);
	if (pResource == nullptr)
	{
		ELOG("ResourceRegistry::CreateTransient() failed to acquire resource for %s", _name.c_str());
		return InvalidHandle;
	}

	Handle h = static_cast<Handle>(m_Resources.size());
	Entry entry;
	entry.name		= _name;
	entry.pResource = pResource;
	m_Resources.push_back(entry);

	return h;
}

RG::Handle RG::ResourceRegistry::CreateTransient(
	ID3D12Device*					_pDevice,
	const std::string&				_name,
	const TransientResourceDesc&	_desc, 
	RHI::TransientResourcePool*		_pResourcePool, 
	RHI::DescriptorPool*			_pRTVPool,
	RHI::DescriptorPool*			_pSRVPool)
{
	RHI::TransientResourcePool::Desc poolDesc;
	poolDesc.Width			= _desc.Width;
	poolDesc.Height			= _desc.Height;
	poolDesc.Format			= _desc.Format;
	poolDesc.ClearColor[0]	= _desc.ClearColor[0];
	poolDesc.ClearColor[1]	= _desc.ClearColor[1];
	poolDesc.ClearColor[2]	= _desc.ClearColor[2];
	poolDesc.ClearColor[3]	= _desc.ClearColor[3];

	auto* pResource = _pResourcePool->Acquire(poolDesc);
	if (pResource == nullptr)
	{
		ELOG("ResourceRegistry::CreateTransient() failed to acquire resource for %s", _name.c_str());
		return InvalidHandle;
	}

	// RTVディスクリプタの発行
	auto* pRTVHandle = _pRTVPool->AllocHandle();
	if (pRTVHandle == nullptr)
	{
		ELOG("ResourceRegistry::CreateTransient() failed to allocate RTV handle for %s", _name.c_str());
		return InvalidHandle;
	}
	_pDevice->CreateRenderTargetView(pResource, nullptr, pRTVHandle->HandleCPU);

	// SRVディスクリプタの発行
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format					= _desc.Format;
	srvDesc.ViewDimension			= D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels		= 1;

	auto* pSRVHandle = _pSRVPool->AllocHandle();
	if (pSRVHandle == nullptr)
	{
		ELOG("ResourceRegistry::CreateTransient() failed to allocate SRV handle for %s", _name.c_str());
		_pRTVPool->FreeHandle(pRTVHandle);
		return InvalidHandle;
	}
	_pDevice->CreateShaderResourceView(pResource, &srvDesc, pSRVHandle->HandleCPU);

	// レジストリに登録
	Handle h = static_cast<Handle>(m_Resources.size());
	Entry entry;
	entry.name			= _name;
	entry.pResource		= pResource;
	entry.pRTVHandle	= pRTVHandle;
	entry.pSRVHandle	= pSRVHandle;
	entry.pRTVPool		= _pRTVPool;
	entry.pSRVPool		= _pSRVPool;
	m_Resources.push_back(entry);
	return h;
}

RG::Handle RG::ResourceRegistry::CreateTransient(
	ID3D12Device*					_pDevice,
	const std::string&				_name, 
	const TransientResourceDesc&	_desc, 
	RHI::TransientResourcePool*		_pResourcePool, 
	RHI::DescriptorPool*			_pRTVPool)
{
	RHI::TransientResourcePool::Desc poolDesc;
	poolDesc.Width			= _desc.Width;
	poolDesc.Height			= _desc.Height;
	poolDesc.Format			= _desc.Format;
	poolDesc.ClearColor[0]	= _desc.ClearColor[0];
	poolDesc.ClearColor[1]	= _desc.ClearColor[1];
	poolDesc.ClearColor[2]	= _desc.ClearColor[2];
	poolDesc.ClearColor[3]	= _desc.ClearColor[3];

	auto* pResource = _pResourcePool->Acquire(poolDesc);
	if (pResource == nullptr)
	{
		ELOG("ResourceRegistry::CreateTransient() failed to acquire resource for %s", _name.c_str());
		return InvalidHandle;
	}

	// RTVディスクリプタの発行
	auto* pRTVHandle = _pRTVPool->AllocHandle();
	if (pRTVHandle == nullptr)
	{
		ELOG("ResourceRegistry::CreateTransient() failed to allocate RTV handle for %s", _name.c_str());
		return InvalidHandle;
	}
	_pDevice->CreateRenderTargetView(pResource, nullptr, pRTVHandle->HandleCPU);

	// レジストリに登録
	Handle h = static_cast<Handle>(m_Resources.size());
	Entry entry;
	entry.name			= _name;
	entry.pResource		= pResource;
	entry.pRTVHandle	= pRTVHandle;
	entry.pSRVHandle	= nullptr;
	entry.pRTVPool		= _pRTVPool;
	entry.pSRVPool		= nullptr;
	m_Resources.push_back(entry);
	return h;
}

// -------------------------------------------------------------------------------
//		IDに応じたリソースを取得
// -------------------------------------------------------------------------------
ID3D12Resource* RG::ResourceRegistry::GetResource(Handle _handle) const
{
	if (_handle >= m_Resources.size()) 
	{ return nullptr; }

	return m_Resources[_handle].pResource;
}

// -------------------------------------------------------------------------------
//		リソースの名前を取得
// -------------------------------------------------------------------------------
const std::string& RG::ResourceRegistry::GetName(Handle _handle) const
{
	static const std::string kInvalid = "Invalid";
	if (_handle >= m_Resources.size()) { return kInvalid; }

	return m_Resources[_handle].name;
}

// -------------------------------------------------------------------------------
//		RTVのハンドルを返す
// -------------------------------------------------------------------------------
RHI::DescriptorHandle* RG::ResourceRegistry::GetRTV(Handle _handle) const
{
	if (_handle >= m_Resources.size()) { return nullptr; }
	return m_Resources[_handle].pRTVHandle;
}

// -------------------------------------------------------------------------------
//		SRVのハンドルを返す
// -------------------------------------------------------------------------------
RHI::DescriptorHandle* RG::ResourceRegistry::GetSRV(Handle _handle) const
{
	if (_handle >= m_Resources.size()) { return nullptr; }
	return m_Resources[_handle].pSRVHandle;
}

// -------------------------------------------------------------------------------
//		レンダーグラフのリセット
// -------------------------------------------------------------------------------
void RG::ResourceRegistry::Clear()
{
	// TransientResourceが確保したディスクリプタだけ解放
	// リソース本体はTransientResourcePool::ReleaseAll側で別途解放される
	for(auto& entry : m_Resources)
	{
		if (entry.pRTVHandle != nullptr && entry.pRTVPool != nullptr)
		{
			entry.pRTVPool->FreeHandle(entry.pRTVHandle);
		}
		if (entry.pSRVHandle != nullptr && entry.pSRVPool != nullptr)
		{
			entry.pSRVPool->FreeHandle(entry.pSRVHandle);
		}
	}
	m_Resources.clear();
}
