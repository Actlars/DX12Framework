// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Material.h"
#include <filesystem>
#include <Framework/Utility/Debug/Logger/Logger.h>

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
    ID3D12Device* _pDevice,
    ID3D12CommandQueue* _pQueue,
    DescriptorPool* _pPool,
    const ResMaterial& _resMat)
{
    if (_pDevice == nullptr || _pQueue == nullptr || _pPool == nullptr)
    {
        return false;
    }

    m_pPool = _pPool;
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

        auto hr = _pDevice->CreateCommittedResource(
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
        _pDevice->CreateConstantBufferView(&cbvDesc, m_pCBHandle->HandleCPU);
    }

    // ─── テクスチャのロード ───
    // テクスチャパスが空またはファイルが存在しない場合はダミーを使う
    const std::wstring* texPaths[TEXTURE_COUNT] =
    {
        &_resMat.DiffuseMap,
        &_resMat.NormalMap,
        &_resMat.SpecularMap,
        &_resMat.ShininessMap,
    };

    // ダミーテクスチャを先に生成しておく（全テクスチャの初期値として使う）
    Texture dummy;
    if (!CreateDummyTexture(_pDevice, _pQueue, _pPool))
    {
        ELOG("Material::Init() Dummy texture creation failed.");
        return false;
    }

    for (auto i = 0u; i < TEXTURE_COUNT; ++i)
    {
        const auto& path = *texPaths[i];

        // パスが空またはファイルが存在しない場合はダミーを使う
        if (path.empty() || !std::filesystem::exists(path))
        {
            m_HasTexture[i] = false;
            // ダミーテクスチャで Init（1x1 白）
            // ここでは空パスを渡すとロード失敗するので
            // CreateDummyTexture で別途生成したものを使う
            continue;
        }

        if (!m_Textures[i].Init(_pDevice, _pQueue, _pPool, path))
        {
            ELOG("Material::Init() Texture[%u] load failed. path=%ls", i, path.c_str());
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

    return m_Textures[_type].GetHandleGPU();
}

MaterialCB* Material::GetCBPtr() const
{
    return m_pMappedPtr;
}

// -------------------------------------------------------------------------------
//      ダミーテクスチャの生成（1x1 白）
// -------------------------------------------------------------------------------
bool Material::CreateDummyTexture(
    ID3D12Device* _pDevice,
    ID3D12CommandQueue* _pQueue,
    DescriptorPool* _pPool)
{
    // テクスチャが存在しないスロットにダミーを差し込む
    // シェーダー側で if(hasTexture) のような分岐なしに
    // 全スロットをサンプリングできるようにするため

    for (auto i = 0u; i < TEXTURE_COUNT; ++i)
    {
        if (m_HasTexture[i])
        {
            continue;
        }

        // 一時ファイルを経由せず、1x1 白テクスチャを DirectXTex で直接生成する
        // Texture クラスは現状ファイルロードのみ対応なので、
        // ここでは 1x1 DEFAULT ヒープリソースを直接作成して SRV を生成する

        // ─── 1x1 RGBA 白データ ───
        const uint32_t whitePixel = 0xFFFFFFFF;

        // DEFAULT ヒープにリソース生成
        D3D12_HEAP_PROPERTIES heapDefault = {};
        heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC resDesc = {};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resDesc.Width = 1;
        resDesc.Height = 1;
        resDesc.DepthOrArraySize = 1;
        resDesc.MipLevels = 1;
        resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        resDesc.SampleDesc.Count = 1;
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ComPtr<ID3D12Resource> pResource;
        auto hr = _pDevice->CreateCommittedResource(
            &heapDefault,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(pResource.GetAddressOf()));
        if (FAILED(hr))
        {
            return false;
        }

        // UPLOAD 中間バッファ
        const UINT64 uploadSize = GetRequiredIntermediateSize(pResource.Get(), 0, 1);
        D3D12_HEAP_PROPERTIES heapUpload = {};
        heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC uploadDesc = {};
        uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDesc.Width = uploadSize;
        uploadDesc.Height = 1;
        uploadDesc.DepthOrArraySize = 1;
        uploadDesc.MipLevels = 1;
        uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
        uploadDesc.SampleDesc.Count = 1;
        uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ComPtr<ID3D12Resource> pUpload;
        hr = _pDevice->CreateCommittedResource(
            &heapUpload,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(pUpload.GetAddressOf()));
        if (FAILED(hr))
        {
            return false;
        }

        // UploadSubresources でコピー
        D3D12_SUBRESOURCE_DATA subresource = {};
        subresource.pData = &whitePixel;
        subresource.RowPitch = 4;     // 1px × 4byte
        subresource.SlicePitch = 4;

        ComPtr<ID3D12CommandAllocator>    pAlloc;
        ComPtr<ID3D12GraphicsCommandList> pCmd;
        ComPtr<ID3D12Fence>               pFence;

        _pDevice->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(pAlloc.GetAddressOf()));
        _pDevice->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            pAlloc.Get(), nullptr,
            IID_PPV_ARGS(pCmd.GetAddressOf()));

        UpdateSubresources(pCmd.Get(), pResource.Get(), pUpload.Get(), 0, 0, 1, &subresource);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = pResource.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        pCmd->ResourceBarrier(1, &barrier);
        pCmd->Close();

        ID3D12CommandList* ppLists[] = { pCmd.Get() };
        _pQueue->ExecuteCommandLists(1, ppLists);

        _pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(pFence.GetAddressOf()));
        HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        _pQueue->Signal(pFence.Get(), 1);
        pFence->SetEventOnCompletion(1, hEvent);
        WaitForSingleObject(hEvent, INFINITE);
        CloseHandle(hEvent);

        // SRV を DescriptorPool に登録
        auto* pHandle = _pPool->AllocHandle();
        if (pHandle == nullptr)
        {
            return false;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        _pDevice->CreateShaderResourceView(pResource.Get(), &srvDesc, pHandle->HandleCPU);

        // Texture クラスに直接注入できないため、
        // ここでは GPU ハンドルだけを m_Textures[i] の代わりに保持する方法が必要。
        // 現状の Texture クラスはファイルロード専用のため、
        // TODO: Texture クラスに InitFromResource() を追加することを推奨
        // 暫定: ダミー用に専用のハンドルキャッシュを持つ
        // （Texture クラスの拡張は次のステップで対応する）
    }

    return true;
}
