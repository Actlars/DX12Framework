// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Material.h"
#include <filesystem>
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
//      コンストラクタ
// -------------------------------------------------------------------------------
Material::Material() 
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//      デストラクタ
// -------------------------------------------------------------------------------
Material::~Material()
{ Term(); }

// -------------------------------------------------------------------------------
//      初期化
// -------------------------------------------------------------------------------
bool Material::Init(
    RHI::Device*        _pRHIDevice,
    const ResMaterial&  _resMat)
{
    if (_pRHIDevice == nullptr)
    {
        return false;
    }

    m_pDevice = _pRHIDevice;

    auto* pDevice = _pRHIDevice->GetDevice();
    auto* pQueue = _pRHIDevice->GetQueue();
    auto* pPool = _pRHIDevice->GetPool(RHI::Device::POOL_TYPE_RES);

    m_pPool = pPool;
    m_pPool->AddRef();

    // ─── 定数バッファの生成 ───
    // D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT (256) バイト境界に合わせる
    // MaterialCB は alignas(256) なので sizeof がすでに 256 の倍数になっている
    {
        D3D12_HEAP_PROPERTIES heapProp = {};
        heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = sizeof(MaterialCB);
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        auto hr = pDevice->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_pCB.GetAddressOf()));
        if (FAILED(hr))
        {
            ELOG("Material::Init() ConstantBuffer creation failed.");
            return false;
        }

        // Map したまま保持する（UPLOADヒープは常時マップが推奨）
        hr = m_pCB->Map(0, nullptr, reinterpret_cast<void**>(&m_pMappedPtr));
        if (FAILED(hr))
        {
            ELOG("Material::Init() ConstantBuffer Map failed.");
            return false;
        }

        // ResMaterial のパラメータを定数バッファに書き込む
        m_pMappedPtr->Diffuse = _resMat.Diffuse;
        m_pMappedPtr->Specular = _resMat.Specular;
        m_pMappedPtr->Emissive = _resMat.Emissive;
        m_pMappedPtr->Alpha = _resMat.Alpha;
        m_pMappedPtr->Shininess = _resMat.Shininess;

        // CBV を DescriptorPool に登録
        m_pCBHandle = m_pPool->AllocHandle();
        if (m_pCBHandle == nullptr)
        {
            ELOG("Material::Init() DescriptorPool::AllocHandle() failed.");
            return false;
        }

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_pCB->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = static_cast<UINT>(sizeof(MaterialCB));
        pDevice->CreateConstantBufferView(&cbvDesc, m_pCBHandle->HandleCPU);
    }

    // ─── テクスチャのロード ───
    // テクスチャパスが空またはファイルが存在しない場合はダミーを使う
    const std::wstring* texPaths[TEXTURE_COUNT] =
    {
        &_resMat.DiffuseMap,&_resMat.NormalMap,
        &_resMat.SpecularMap, &_resMat.ShininessMap,
    };

    for (auto i = 0u; i < TEXTURE_COUNT; ++i)
    {
        const auto& path = *texPaths[i];

        // パスが空 → ダミーは作らずフラグだけ落とす
        // GetTextureHandle / GetTextureIndex が Device::GetDummyTexture() を参照する
        if (path.empty() || !std::filesystem::exists(path))
        {
            m_HasTexture[i] = false;
            continue;
        }

        if (!m_Textures[i].Init(pDevice, pQueue, pPool, path))
        {
            ELOG("Material::Init() Texture[%u] load failed path = %ls", i, path.c_str());
            m_HasTexture[i] = false;
            continue;
        }

        m_HasTexture[i] = true;
    }

    return true;
}

// -------------------------------------------------------------------------------
//      終了処理
// -------------------------------------------------------------------------------
void Material::Term()
{
    // テクスチャを解放（Texture::Term() 内で SRV ハンドルをプールに返却）
    for (auto& tex : m_Textures)
    {
        tex.Term();
    }

    // 定数バッファのアンマップ
    if (m_pCB != nullptr)
    {
        m_pCB->Unmap(0, nullptr);
        m_pCB.Reset();
    }
    m_pMappedPtr = nullptr;

    // CBV ハンドルをプールに返却
    if (m_pPool != nullptr && m_pCBHandle != nullptr)
    {
        m_pPool->FreeHandle(m_pCBHandle);
        m_pCBHandle = nullptr;
    }

    // プールの参照カウントを減らす
    if (m_pPool != nullptr)
    {
        m_pPool->Release();
        m_pPool = nullptr;
    }
}

D3D12_GPU_VIRTUAL_ADDRESS Material::GetCBAddress() const
{
    if (m_pCB == nullptr)
    {
        return 0;
    }
    return m_pCB->GetGPUVirtualAddress();
}

D3D12_GPU_DESCRIPTOR_HANDLE Material::GetTextureHandle(TextureType _type) const
{
    if (_type >= TEXTURE_COUNT)
    {
        return D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
    }

    if (!m_HasTexture[_type])
    {
        auto* pDummy = m_pDevice->GetDummyTexture();
        return pDummy != nullptr ? pDummy->GetHandleGPU() : D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
    }

    return m_Textures[_type].GetHandleGPU();
}

uint32_t Material::GetTextureIndex(TextureType _type) const
{
    if (_type >= TEXTURE_COUNT) 
    { return 0; }

    if (!m_HasTexture[_type])
    {
        auto* pDummy = m_pDevice->GetDummyTexture();
        return pDummy != nullptr ? pDummy->GetIndex() : 0;
    }

    return m_Textures[_type].GetIndex();
}

MaterialCB* Material::GetCBPtr() const
{
    return m_pMappedPtr;
}
