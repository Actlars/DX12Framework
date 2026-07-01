// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "GameScene.h"
#include <Engine/Utility/Debug/Logger/Logger.h>
#include <Engine/Utility/FileUtil/FileUtil.h>
#include <Engine/Mesh/MeshLoader/MeshLoader.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

// -------------------------------------------------------------------------------
// コンストラクタ
// -------------------------------------------------------------------------------
GameScene::GameScene(const Desc& _desc)
    : m_Desc(_desc)
{ /* DO_NOTHING */
}

// -------------------------------------------------------------------------------
// 初期化
// -------------------------------------------------------------------------------
bool GameScene::OnInit(GraphicsDevice* _pGraphicsDevice)
{
    assert(_pGraphicsDevice != nullptr);
    m_pGraphicsDevice = _pGraphicsDevice;

    auto* pDevice = m_pGraphicsDevice->GetDevice();

    if (!InitRootSignature(pDevice)) { ELOG("InitRootSignature() failed."); return false; }
    if (!InitPipelineState(pDevice)) { ELOG("InitPipelineState() failed."); return false; }

    InitCamera();

    if (!InitMeshes()) { ELOG("InitMeshes() failed."); return false; }
    if (!InitSampler()) { ELOG("InitSampler() failed."); return false; }
    if (!InitGameObjects()) { ELOG("InitGameObjects() failed."); return false; }

    ShowCursor(FALSE);
    m_IsInitialized = true;
    return true;
}

// -------------------------------------------------------------------------------
// 終了処理
// -------------------------------------------------------------------------------
void GameScene::OnTerm()
{
    ShowCursor(TRUE);

    // GameObjectManager を先に解放する
    // MeshComponent が Mesh / Material を参照しているため
    m_MeshComponents.clear();
    m_ObjectManager.Clear();

    m_Sampler.Term();
    m_Materials.clear();
    m_Meshes.clear();

    m_pPSO.Reset();
    m_pRootSignature.Reset();

    m_pGraphicsDevice   = nullptr;
    m_IsInitialized     = false;
}

// -------------------------------------------------------------------------------
// 毎フレームの更新処理
// -------------------------------------------------------------------------------
void GameScene::OnUpdate(float _deltaTime)
{
    // キーボード・マウス入力でカメラを更新する
    UpdateInput(_deltaTime);

    // 全 MeshComponent に最新のカメラ行列とフレームインデックスを渡す
    UpdateViewProj();

    // 全 GameObject の Update を呼ぶ
    m_ObjectManager.Update(_deltaTime);
}

// -------------------------------------------------------------------------------
// 毎フレームの描画コマンド組み立て
// -------------------------------------------------------------------------------
void GameScene::OnRender(ID3D12GraphicsCommandList* _pCmd)
{
    if (_pCmd == nullptr) { return; }

    // ─── RootSignature / PSO のセット ───
    _pCmd->SetGraphicsRootSignature(m_RootSignatureLayout.GetRootSignature());
    _pCmd->SetPipelineState(m_pPSO.Get());

    // ─── DescriptorHeap のセット ───
    // RES（CBV/SRV）と SMP（Sampler）の両方を設定する必要がある
    ID3D12DescriptorHeap* heaps[] =
    {
        m_pGraphicsDevice->GetPool(GraphicsDevice::POOL_TYPE_RES)->GetHeap(),
        m_pGraphicsDevice->GetPool(GraphicsDevice::POOL_TYPE_SMP)->GetHeap(),
    };
    _pCmd->SetDescriptorHeaps(_countof(heaps), heaps);

    // ─── サンプラーのバインド ───
    // サンプラーは全メッシュ共通なのでループの外でバインドする
    _pCmd->SetGraphicsRootDescriptorTable(
        m_RootSignatureLayout.GetSlot("Sampler"),
        m_Sampler.GetHandleGPU());

    // 描画用データ収集フェーズ（マルチスレッド可）
    m_ObjectManager.Submit(&m_RenderQueue);

    // コマンド発行フェーズ（単一スレッド）
    m_RenderQueue.Execute(_pCmd);

    // ─── フレーム末の削除処理 ───
    m_ObjectManager.FlushPendingRemoves();
}

// ===============================================================================
// private
// ===============================================================================

// -------------------------------------------------------------------------------
// RootSignature 生成
// -------------------------------------------------------------------------------
bool GameScene::InitRootSignature(ID3D12Device* _pDevice)
{
    {
        //D3D12_DESCRIPTOR_RANGE srvRange = {};
        //srvRange.RangeType                          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        //srvRange.NumDescriptors                     = 1;
        //srvRange.BaseShaderRegister                 = 0; // t0
        //srvRange.RegisterSpace                      = 0;
        //srvRange.OffsetInDescriptorsFromTableStart  = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        //D3D12_DESCRIPTOR_RANGE smpRange = {};
        //smpRange.RangeType                          = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        //smpRange.NumDescriptors                     = 1;
        //smpRange.BaseShaderRegister                 = 0; // s0
        //smpRange.RegisterSpace                      = 0;
        //smpRange.OffsetInDescriptorsFromTableStart  = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        //D3D12_ROOT_PARAMETER params[4] = {};

        //// [0] b0: 変換行列（VS）
        //params[ROOT_PARAM_CBV_TRANSFORM].ParameterType              = D3D12_ROOT_PARAMETER_TYPE_CBV;
        //params[ROOT_PARAM_CBV_TRANSFORM].Descriptor.ShaderRegister  = 0;
        //params[ROOT_PARAM_CBV_TRANSFORM].Descriptor.RegisterSpace   = 0;
        //params[ROOT_PARAM_CBV_TRANSFORM].ShaderVisibility           = D3D12_SHADER_VISIBILITY_VERTEX;

        //// [1] b1: マテリアルパラメータ（PS）
        //params[ROOT_PARAM_CBV_MATERIAL].ParameterType               = D3D12_ROOT_PARAMETER_TYPE_CBV;
        //params[ROOT_PARAM_CBV_MATERIAL].Descriptor.ShaderRegister   = 1;
        //params[ROOT_PARAM_CBV_MATERIAL].Descriptor.RegisterSpace    = 0;
        //params[ROOT_PARAM_CBV_MATERIAL].ShaderVisibility            = D3D12_SHADER_VISIBILITY_PIXEL;

        //// [2] t0: ディフューズテクスチャ（PS）
        //params[ROOT_PARAM_SRV_DIFFUSE].ParameterType                        = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        //params[ROOT_PARAM_SRV_DIFFUSE].DescriptorTable.NumDescriptorRanges  = 1;
        //params[ROOT_PARAM_SRV_DIFFUSE].DescriptorTable.pDescriptorRanges    = &srvRange;
        //params[ROOT_PARAM_SRV_DIFFUSE].ShaderVisibility                     = D3D12_SHADER_VISIBILITY_PIXEL;

        //// [3] s0: サンプラー（PS）
        //params[ROOT_PARAM_SMP].ParameterType                        = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        //params[ROOT_PARAM_SMP].DescriptorTable.NumDescriptorRanges  = 1;
        //params[ROOT_PARAM_SMP].DescriptorTable.pDescriptorRanges    = &smpRange;
        //params[ROOT_PARAM_SMP].ShaderVisibility                     = D3D12_SHADER_VISIBILITY_PIXEL;

        //auto flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        //flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
        //flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
        //flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        //D3D12_ROOT_SIGNATURE_DESC desc = {};
        //desc.NumParameters      = _countof(params);
        //desc.pParameters        = params;
        //desc.NumStaticSamplers  = 0;
        //desc.pStaticSamplers    = nullptr;
        //desc.Flags              = flags;

        //ComPtr<ID3DBlob> pBlob, pError;
        //auto hr = D3D12SerializeRootSignature(
        //    &desc, D3D_ROOT_SIGNATURE_VERSION_1_0,
        //    pBlob.GetAddressOf(), pError.GetAddressOf());
        //if (FAILED(hr))
        //{
        //    if (pError) { ELOG("RootSignature error: %s", (char*)pError->GetBufferPointer()); }
        //    return false;
        //}

        //hr = _pDevice->CreateRootSignature(
        //    0, pBlob->GetBufferPointer(), pBlob->GetBufferSize(),
        //    IID_PPV_ARGS(m_pRootSignature.GetAddressOf()));
        //if (FAILED(hr)) { ELOG("CreateRootSignature failed."); return false; }

        //return true;
    }

    if (!m_RootSignatureLayout.LoadFromJson(_pDevice, L"Assets/Config/Json/RootSignature/MeshShader.json"))
    {
        ELOG("RootSignatureLayout::LoadFromJson failed");
        return false;
    }
    
    return true;
}

// -------------------------------------------------------------------------------
// PSO 生成
// -------------------------------------------------------------------------------
bool GameScene::InitPipelineState(ID3D12Device* _pDevice)
{
    ComPtr<ID3DBlob> pVS, pPS;
    std::wstring vsPath, psPath;

    if (!SearchFilePath(L"MeshShaderVS.cso", vsPath)) { ELOG("VS not found."); return false; }
    if (!SearchFilePath(L"MeshShaderPS.cso", psPath)) { ELOG("PS not found."); return false; }

    if (FAILED(D3DReadFileToBlob(vsPath.c_str(), pVS.GetAddressOf()))) { return false; }
    if (FAILED(D3DReadFileToBlob(psPath.c_str(), pPS.GetAddressOf()))) { return false; }

    D3D12_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode                 = D3D12_FILL_MODE_SOLID;
    rsDesc.CullMode                 = D3D12_CULL_MODE_BACK;
    rsDesc.FrontCounterClockwise    = FALSE;
    rsDesc.DepthBias                = D3D12_DEFAULT_DEPTH_BIAS;
    rsDesc.DepthBiasClamp           = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rsDesc.SlopeScaledDepthBias     = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rsDesc.DepthClipEnable          = TRUE;

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable     = FALSE;
    blendDesc.IndependentBlendEnable    = FALSE;
    for (auto& rt : blendDesc.RenderTarget)
    {
        rt.BlendEnable              = FALSE;
        rt.SrcBlend                 = D3D12_BLEND_ONE;
        rt.DestBlend                = D3D12_BLEND_ZERO;
        rt.BlendOp                  = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha            = D3D12_BLEND_ONE;
        rt.DestBlendAlpha           = D3D12_BLEND_ZERO;
        rt.BlendOpAlpha             = D3D12_BLEND_OP_ADD;
        rt.LogicOp                  = D3D12_LOGIC_OP_NOOP;
        rt.RenderTargetWriteMask    = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    D3D12_DEPTH_STENCIL_DESC dssDesc = {};
    dssDesc.DepthEnable     = TRUE;
    dssDesc.DepthWriteMask  = D3D12_DEPTH_WRITE_MASK_ALL;
    dssDesc.DepthFunc       = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    dssDesc.StencilEnable   = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout             = ResMeshVertex::InputLayout;
    psoDesc.pRootSignature          = m_RootSignatureLayout.GetRootSignature();
    psoDesc.VS                      = { pVS->GetBufferPointer(), pVS->GetBufferSize() };
    psoDesc.PS                      = { pPS->GetBufferPointer(), pPS->GetBufferSize() };
    psoDesc.RasterizerState         = rsDesc;
    psoDesc.BlendState              = blendDesc;
    psoDesc.DepthStencilState       = dssDesc;
    psoDesc.SampleMask              = UINT_MAX;
    psoDesc.PrimitiveTopologyType   = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets        = 1;
    psoDesc.RTVFormats[0]           = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat               = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count        = 1;

    auto hr = _pDevice->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(m_pPSO.GetAddressOf()));
    if (FAILED(hr)) { ELOG("CreateGraphicsPipelineState failed. hr=0x%08X", hr); return false; }

    return true;
}

// -------------------------------------------------------------------------------
// カメラ初期化
// -------------------------------------------------------------------------------
void GameScene::InitCamera()
{
    FPSCamera::Desc desc;
    desc.Position   = m_Desc.CameraPosition;
    desc.MoveSpeed  = m_Desc.CameraMoveSpeed;
    desc.RotSpeed   = m_Desc.CameraRotSpeed;
    m_Camera.Init(desc);

    m_Camera.SetFov(m_Desc.CameraFov);
    m_Camera.SetAspect(
        static_cast<float>(m_pGraphicsDevice->GetWidth()) /
        static_cast<float>(m_pGraphicsDevice->GetHeight()));
    m_Camera.SetNearFar(m_Desc.CameraNear, m_Desc.CameraFar);
    m_Camera.Update();
}

// -------------------------------------------------------------------------------
// メッシュ・マテリアルのロード
// -------------------------------------------------------------------------------
bool GameScene::InitMeshes()
{
    auto* pDevice   = m_pGraphicsDevice->GetDevice();
    auto* pQueue    = m_pGraphicsDevice->GetQueue();
    auto* pPool     = m_pGraphicsDevice->GetPool(GraphicsDevice::POOL_TYPE_RES);

    std::vector<ResMesh>     resMeshes;
    std::vector<ResMaterial> resMaterials;
    
    m_Desc.ModelPath = L"Assets/Model/Player/Elinyaa/Elinyaa.fbx";

    if (!MeshLoader::Load(m_Desc.ModelPath, resMeshes, resMaterials))
    {
        ELOG("MeshLoader::Load() failed. path=%ls", m_Desc.ModelPath.c_str());
        return false;
    }

    m_Meshes.reserve(resMeshes.size());
    for (auto& resMesh : resMeshes)
    {
        auto mesh = std::make_unique<Mesh>();
        if (!mesh->Init(pDevice, resMesh))
        {
            ELOG("Mesh::Init() failed."); return false;
        }
        m_Meshes.emplace_back(std::move(mesh));
    }

    m_Materials.reserve(resMaterials.size());
    for (auto& resMat : resMaterials)
    {
        auto mat = std::make_unique<Material>();
        if (!mat->Init(pDevice, pQueue, pPool, resMat))
        {
            ELOG("Material::Init() failed."); return false;
        }
        m_Materials.emplace_back(std::move(mat));
    }

    return true;
}

// -------------------------------------------------------------------------------
// サンプラー初期化
// -------------------------------------------------------------------------------
bool GameScene::InitSampler()
{
    auto* pDevice   = m_pGraphicsDevice->GetDevice();
    auto* pSmpPool  = m_pGraphicsDevice->GetPool(GraphicsDevice::POOL_TYPE_SMP);

    if (!m_Sampler.Init(pDevice, pSmpPool, Sampler::CreateLinearWrap()))
    {
        ELOG("Sampler::Init() failed."); return false;
    }

    return true;
}

// -------------------------------------------------------------------------------
// GameObjectManager への登録
// -------------------------------------------------------------------------------
bool GameScene::InitGameObjects()
{
    auto* pDevice   = m_pGraphicsDevice->GetDevice();
    auto* pPool     = m_pGraphicsDevice->GetPool(GraphicsDevice::POOL_TYPE_RES);
    const auto fc   = m_pGraphicsDevice->GetFrameCount();

    // 各メッシュに対して GameObject を1つ生成する
    for (auto i = 0u; i < m_Meshes.size(); ++i)
    {
        const auto materialId = m_Meshes[i]->GetMaterialId();
        ELOG("Mesh[%u] matId=%u, materials.size=%zu", i, materialId, m_Materials.size());

        Material* pMaterial = (materialId < m_Materials.size()) ? m_Materials[materialId].get() : nullptr;
        ELOG("Mesh[%u] pMat=%s", i, pMaterial ? "valid" : "nullptr");

        const auto name = "Mesh_" + std::to_string(i);
        auto* pObj = m_ObjectManager.Add<GameObject>(name);

        // TransformComponent の追加
        auto* pTransform = pObj->AddComponent<TransformComponent>();
        pTransform->SetPosition({ 0.0f, 0.0f, 0.0f });
        pTransform->SetScale({ 1.0f, 1.0f, 1.0f });

        // MeshComponent の追加
        auto* pMeshComp = pObj->AddComponent<MeshComponent>();

        // 定数バッファの初期化（FrameCount 分）
        if (!pMeshComp->Init(pDevice, pPool, fc))
        {
            ELOG("MeshComponent::Init() failed. index=%u", i);
            return false;
        }

        // メッシュとマテリアルを設定
        const auto matId = m_Meshes[i]->GetMaterialId();
        Material* pMat = (matId < m_Materials.size()) ? m_Materials[matId].get() : nullptr;
        pMeshComp->SetMesh(m_Meshes[i].get(), pMat);

        // ── 一時テスト: マテリアルごとに別色を流し込む ──
        static const DirectX::XMFLOAT3 kDebug[] = {
            {1,0,0},{0,1,0},{0,0,1},{1,1,0},{1,0,1},{0,1,1}
        };
        if (auto* cb = pMat->GetCBPtr())
        {
            cb->Diffuse = kDebug[matId % 6];
        }

        // RootSignature のスロット番号を設定（GameScene の定義と合わせる）
        pMeshComp->SetRootLayout(&m_RootSignatureLayout);

        // UpdateViewProj() で使うためにキャッシュしておく
        m_MeshComponents.emplace_back(pMeshComp);
    }

    ELOG("InitGameObjects: mesh=%zu objects=%zu",
        m_Meshes.size(), m_ObjectManager.ObjectCount());

    return true;
}

// -------------------------------------------------------------------------------
// 入力処理とカメラ更新
// -------------------------------------------------------------------------------
void GameScene::UpdateInput(float _deltaTime)
{
    if (GetAsyncKeyState('W') & 0x8000) { m_Camera.MoveForward(_deltaTime); }
    if (GetAsyncKeyState('S') & 0x8000) { m_Camera.MoveBack(_deltaTime); }
    if (GetAsyncKeyState('A') & 0x8000) { m_Camera.MoveLeft(_deltaTime); }
    if (GetAsyncKeyState('D') & 0x8000) { m_Camera.MoveRight(_deltaTime); }
    if (GetAsyncKeyState('E') & 0x8000) { m_Camera.MoveUp(_deltaTime); }
    if (GetAsyncKeyState('Q') & 0x8000) { m_Camera.MoveDown(_deltaTime); }

    HWND hWnd = GetActiveWindow();
    if (hWnd == nullptr) { return; }

    RECT rect;
    GetWindowRect(hWnd, &rect);
    const LONG cx = (rect.left + rect.right) / 2;
    const LONG cy = (rect.top + rect.bottom) / 2;

    POINT mousePos;
    GetCursorPos(&mousePos);
    m_Camera.AddYaw(static_cast<float>(mousePos.x - cx));
    m_Camera.AddPitch(static_cast<float>(mousePos.y - cy));
    SetCursorPos(cx, cy);

    m_Camera.Update();
}

// -------------------------------------------------------------------------------
// 全 MeshComponent にカメラ行列とフレームインデックスを渡す
// -------------------------------------------------------------------------------
void GameScene::UpdateViewProj()
{
    const auto frameIndex = m_pGraphicsDevice->GetFrameIndex();
    const auto view = m_Camera.GetView();
    const auto proj = m_Camera.GetProj();

    for (auto* pMeshComp : m_MeshComponents)
    {
        if (pMeshComp == nullptr) { continue; }
        pMeshComp->SetFrameIndex(frameIndex);
        pMeshComp->SetViewProj(view, proj);
    }
}