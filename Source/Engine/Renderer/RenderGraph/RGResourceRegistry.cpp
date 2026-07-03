// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "RGResourceRegistry.h"

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
//		レンダーグラフのリセット
// -------------------------------------------------------------------------------
void RG::ResourceRegistry::Clear()
{
	m_Resources.clear();
}
