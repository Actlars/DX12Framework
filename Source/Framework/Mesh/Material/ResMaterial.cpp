//// -------------------------------------------------------------------------------
//// Includes
//// -------------------------------------------------------------------------------
//#include "Material.h"
//#include <Framework/Utility/FileUtil/FileUtil.h>
//#include <Framework/Debug/Logger/Logger.h>
//
//namespace {
//
//	// -------------------------------------------------------------------------------
//	// Constant Values
//	// -------------------------------------------------------------------------------
//	constexpr wchar_t* DummyTag = L"";
//
//}// namespace
//
//// -------------------------------------------------------------------------------
//// Material class
//// -------------------------------------------------------------------------------
//
//// -------------------------------------------------------------------------------
////		コンストラクタ
//// -------------------------------------------------------------------------------
//Material::Material()
//: m_pDevice(nullptr)
//, m_pPool(nullptr)
//{ /* DO_NOTHING */ }
//
//// -------------------------------------------------------------------------------
////		デストラクタ
//// -------------------------------------------------------------------------------
//Material::~Material() 
//{ Term(); }
//
//
//// -------------------------------------------------------------------------------
////		初期化処理
//// -------------------------------------------------------------------------------
//bool Material::Init
//(
//	ID3D12Device*	_pDevice,
//	DescriptorPool* _pPool,
//	size_t			_bufferSize,
//	size_t			_count
//)
//{
//	if (_pDevice == nullptr || _pPool == nullptr || _count == 0)
//	{
//		ELOG("Error : Invalid Argument.");
//		return false;
//	}
//
//	Term();
//
//	m_pDevice = _pDevice;
//	m_pDevice->AddRef();
//
//	m_pPool = _pPool;
//	m_pPool->AddRef();
//
//	m_Subset.resize(_count);
//
//	// ダミーテクスチャ生成
//	{
//		auto pTexture = new(std::nothrow) Texture();;
//		if (pTexture == nullptr) 
//		{ return false; }
//
//		D3D12_RESOURCE_DESC desc = {};
//		desc.Dimension			= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
//		desc.Width				= 1;
//		desc.Height				= 1;
//		desc.DepthOrArraySize	= 1;
//		desc.MipLevels			= 1;
//		desc.Format				= DXGI_FORMAT_R8G8B8A8_UNORM;
//		desc.SampleDesc.Count	= 1;
//		desc.SampleDesc.Quality = 0;
//
//		if (!pTexture->Init(_pDevice, _pPool, &desc, false))
//		{
//			ELOG("Error : Texture::Init() Failed");
//			pTexture->Term();
//			delete pTexture;
//			return false;
//		}
//
//		m_pTexture[DummyTag] = pTexture;
//	}
//
//}
//
//
//
