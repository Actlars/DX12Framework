// -------------------------------------------------------------------------------
//		Includes
// -------------------------------------------------------------------------------
#include "SceneRenderer.h"
#include <Engine/RHI/Core/Device/Device.h>
#include <Engine/GameObject/GameObjectManager.h>
#include <Engine/Camera/FPSCamera/FPSCamera.h>
#include <Engine/GameObject/Components/MeshComponent/MeshComponent.h>
#include <Engine/Mesh/MeshLoader/MeshLoader.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
SceneRenderer::SceneRenderer() 
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
SceneRenderer::~SceneRenderer() 
{ Term(); }

// -------------------------------------------------------------------------------
//      初期化
// -------------------------------------------------------------------------------
bool SceneRenderer::Init(RHI::Device* _pDevice)
{
	m_pDevice = _pDevice;
	auto* pDevice = _pDevice->GetDevice();

	if (!m_MeshRootSignatureLayout.LoadFromJson(pDevice, L"Assets/Config/Json/RootSignature/MeshShader.json"))
	{ 
        ELOG("SceneRenderer::Init() : MeshRootSignatureLayout load failed");
        return false;
    }

    m_pMeshPSO = m_pDevice->GetPipelineCache()->GetOrCreate(
        pDevice,
        L"Assets/Config/Json/PipelineState/MeshShader.json",
        m_MeshRootSignatureLayout.GetRootSignature(),
        ResMeshVertex::InputLayout);

    if (m_pMeshPSO == nullptr)
    {
        ELOG("InitPipelineState() failed");
        return false;
    }

    if (!m_MeshRootSignatureLayoutBindless.LoadFromJson(pDevice, L"Assets/Config/Json/RootSignature/MeshStandardShaderBindless.json"))
    {
        ELOG("SceneRenderer::Init() : MeshRootSignatureLayoutBindless load failed");
        return false;
    }

    m_pMeshPSOBindless = m_pDevice->GetPipelineCache()->GetOrCreate(
        pDevice, L"Assets/Config/Json/PipelineState/MeshStandardShaderBindless.json",
        m_MeshRootSignatureLayoutBindless.GetRootSignature(), ResMeshVertex::InputLayout);

    if (m_pMeshPSOBindless == nullptr)
    {
        ELOG("InitPipelineState() failed");
        return false;
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // ─── Mesh Shader 三角形テスト ───
    //if (!m_TriangleRootSignatureLayout.LoadFromJson(pDevice, L"Assets/Config/Json/RootSignature/TriangleMeshShaderRootSignature.json"))
    //{
    //    ELOG("SceneRenderer::Init() : TriangleRootSignatureLayout load failed");
    //    return false;
    //}

    //m_pTrianglePSO = m_pDevice->GetPipelineCache()->GetOrCreate(
    //    pDevice, L"Assets/Config/Json/PipelineState/Triangle.json",
    //    m_TriangleRootSignatureLayout.GetRootSignature(),
    //    D3D12_INPUT_LAYOUT_DESC{ nullptr, 0 });   // Mesh ShaderはInputLayout不要

    //if (m_pTrianglePSO == nullptr)
    //{
    //    ELOG("SceneRenderer::Init() : Triangle PSO creation failed");
    //    return false;
    //}

    //// 三角形データ
    //struct TriangleVertex
    //{
    //    DirectX::XMFLOAT3 Position;
    //    DirectX::XMFLOAT4 Color;
    //};

    //static const TriangleVertex kVertices[3] =
    //{
    //    { {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
    //    { {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
    //    { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
    //};
    //static const uint32_t kIndices[3] = { 0, 1, 2 };

    //MeshletResource::MeshletVertexData vertices{ kVertices, sizeof(TriangleVertex), 3 };
    //MeshletResource::MeshletIndexData  indices{ kIndices, 3 };

    //if (!m_TriangleMesh.Init(pDevice, vertices, indices))
    //{
    //    ELOG("SceneRenderer::Init() : TriangleMesh Init failed");
    //    return false;
    //}

    //m_TriangleMesh.SetRootSlots(
    //    m_TriangleRootSignatureLayout.GetSlot("Vertices"),
    //    m_TriangleRootSignatureLayout.GetSlot("Indices"));

    //// Transform用CB（単位行列でクリップ空間にそのまま置く）
    //if (!m_TriangleTransformCB.Init(pDevice, m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES), sizeof(TransformCB)))
    //{
    //    ELOG("SceneRenderer::Init() : TriangleTransformCB Init failed");
    //    return false;
    //}

    //auto* pTransform = m_TriangleTransformCB.GetPtr<TransformCB>();
    //pTransform->World = DirectX::XMMatrixIdentity();
    //pTransform->View = DirectX::XMMatrixIdentity();
    //pTransform->Proj = DirectX::XMMatrixIdentity();

    //////////////////////////////////////////////////////////////////////////////////////////

    // MeshShader 実モデル描画
    if (!m_ModelMeshletRootSignatureLayout.LoadFromJson(pDevice, L"Assets/Config/Json/RootSignature/MeshletShader.json"))
    {
        ELOG("SceneRenderer::Init() : ModelMeshletRootSignatureLayout load failed");
        return false;
    }

    m_pModelMeshletPSO = m_pDevice->GetPipelineCache()->GetOrCreate(
        pDevice, L"Assets/Config/Json/PipelineState/MeshletShader.json",
        m_ModelMeshletRootSignatureLayout.GetRootSignature(),
        D3D12_INPUT_LAYOUT_DESC{ nullptr,0 });

    if (m_pModelMeshletPSO == nullptr)
    {
        ELOG("SceneRenderer::Init() : ModelMeshlet PSO creation failed");
        return false;
    }

    // テスト用にモデルを読み込む
    std::vector<ResMesh> resMeshes;
    std::vector<ResMaterial> resMaterials;
    if (!MeshLoader::Load(L"Assets/Model/Player/Elinyaa/Elinyaa.fbx", resMeshes, resMaterials))
    {
        ELOG("SceneRenderer::Init() : ModelMeshlet MeshLoader::Load failed");
        return false;
    }

    if (resMeshes.empty())
    {
        ELOG("SceneRenderer::Init() : ModelMeshlet resMeshes is empty");
        return false;
    }

    // マテリアル（テクスチャ）を生成
    auto* pQueue = m_pDevice->GetQueue();
    auto* pResPool = m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES);

    m_ModelMaterials.reserve(resMaterials.size());
    for (auto& resMat : resMaterials)
    {
        auto mat = std::make_unique<Material>();
        if (!mat->Init(pDevice, pQueue, pResPool, resMat))
        {
            ELOG("SceneRenderer::Init() : ModelMeshlet Material Init failed");
            return false;
        }
        m_ModelMaterials.emplace_back(std::move(mat));
    }

    // 全メッシュぶんMeshletResourceを作る
    m_ModelMeshletMeshes.reserve(resMeshes.size());
    for (auto& resMesh : resMeshes)
    {
        auto meshlet = std::make_unique<MeshletResource>();
        if (!meshlet->InitMeshlets(pDevice, resMesh))
        {
            ELOG("SceneRenderer::Init() : ModelMeshletMesh InitMeshlets failed");
            continue;
        }

        meshlet->SetMeshletRootSlots(
            m_ModelMeshletRootSignatureLayout.GetSlot("Vertices"),
            m_ModelMeshletRootSignatureLayout.GetSlot("MeshletVertexIndices"),
            m_ModelMeshletRootSignatureLayout.GetSlot("PackedPrimitiveIndices"),
            m_ModelMeshletRootSignatureLayout.GetSlot("Meshlets"));

        ModelMeshletEntry entry;
        entry.Mesh = std::move(meshlet);

        // 対応するマテリアルからテクスチャIndexを取得
        const auto matId = resMesh.MaterialId;
        if (matId < m_ModelMaterials.size())
        {
            entry.DiffuseTextureIndex = m_ModelMaterials[matId]->GetTextureIndex(Material::TEXTURE_DIFFUSE);
        }

        m_ModelMeshletMeshes.emplace_back(std::move(entry));
    }

    // フレームカウント分のTransformCBを作る
    const auto frameCount = m_pDevice->GetFrameCount();
    m_ModelMeshletTransformCBs.reserve(frameCount);
    for (auto i = 0u; i < frameCount; ++i)
    {
        auto cb = std::make_unique<RHI::ConstantBuffer>();
        if (!cb->Init(pDevice, m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES), sizeof(TransformCB)))
        {
            ELOG("SceneRenderer::Init() : ModelMeshletTransformCB[%u] Init failed", i);
            return false;
        }
        m_ModelMeshletTransformCBs.emplace_back(std::move(cb));
    }

    auto* pSmpPool = m_pDevice->GetPool(RHI::Device::POOL_TYPE_SMP);

    if (!m_Sampler.Init(pDevice, pSmpPool, RHI::Sampler::CreateLinearWrap()))
    {
        ELOG("Sampler::Init() failed.");
        return false;
    }

    // AddPostProcessEffectで登録されたエフェクトを、まとめて初期化
    if (!m_PostProcessStack.InitAllEffect(m_pDevice))
    {
        ELOG("PostProcessStack::InitAllEffect() failed");
        return false;
    }


    testIndexCB.Init(m_pDevice->GetDevice(), m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES), sizeof(uint32_t) * 4);

    return true;
}

// -------------------------------------------------------------------------------
//      終了処理
// -------------------------------------------------------------------------------
void SceneRenderer::Term()
{
    m_pMeshPSO  = nullptr;
    m_pDevice   = nullptr;
}

// -------------------------------------------------------------------------------
//      描画
// -------------------------------------------------------------------------------
void SceneRenderer::Render(ID3D12GraphicsCommandList* _pCmd, GameObjectManager& _objects, FPSCamera& _camera)
{
    auto* pTracker = m_pDevice->GetResourceStateTracker();
    const auto frameIndex = m_pDevice->GetFrameIndex();

    // 外部リソースをグラフに取り込む
    auto colorHandle = m_RenderGraph.ImportResource("BackBuffer", m_pDevice->GetColorTarget(frameIndex)->GetResource());
    auto depthHandle = m_RenderGraph.ImportResource("DepthBuffer", m_pDevice->GetDepthTarget()->GetResource());

    // MainPass出力先のTransientバッファー
    RG::TransientResourceDesc sceneColorDesc;
    sceneColorDesc.Width    = m_pDevice->GetWidth();
    sceneColorDesc.Height   = m_pDevice->GetHeight();
    sceneColorDesc.Format   = DXGI_FORMAT_R16G16B16A16_FLOAT;
    sceneColorDesc.ClearColor[0] = sceneColorDesc.ClearColor[1] = 0.0f;
    sceneColorDesc.ClearColor[2] = sceneColorDesc.ClearColor[3] = 1.0f;

    auto sceneColorHandle = m_RenderGraph.GetRegistry().CreateTransient(
        m_pDevice->GetDevice(), "SceneColor", sceneColorDesc,
        m_pDevice->GetTransientResourcePool(),
        m_pDevice->GetPool(RHI::Device::POOL_TYPE_RTV),
        m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES));

    // MainPass : メッシュ通常描画
    m_RenderGraph.AddPass(
        "MainPass",
        [sceneColorHandle, depthHandle](RG::PassBuilder& _builder)
        {
            _builder.Use(sceneColorHandle, D3D12_RESOURCE_STATE_RENDER_TARGET);
            _builder.Use(depthHandle, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        },
        [this, sceneColorHandle, &_objects](ID3D12GraphicsCommandList* cmd, const RG::ResourceRegistry& res)
        {
            auto* pRTV = res.GetRTV(sceneColorHandle);
            auto handleDSV = m_pDevice->GetDepthTarget()->GetHandleDSV()->HandleCPU;
            cmd->OMSetRenderTargets(1, &pRTV->HandleCPU, FALSE, &handleDSV);

            // クリア処理
            const float clearColor[4] = { 0.0f,0.0f,1.0f,1.0f };
            cmd->ClearRenderTargetView(pRTV->HandleCPU, clearColor, 0, nullptr);
            cmd->ClearDepthStencilView(handleDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            ID3D12DescriptorHeap* heaps[] =
            {
                m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES)->GetHeap(),
                m_pDevice->GetPool(RHI::Device::POOL_TYPE_SMP)->GetHeap(),
            };
            cmd->SetDescriptorHeaps(_countof(heaps), heaps);

            // GraphicsRootSignatureの設定
            if (m_RenderMode == RenderMode::Traditional)
            {
                cmd->SetGraphicsRootSignature(m_MeshRootSignatureLayout.GetRootSignature());
                cmd->SetPipelineState(m_pMeshPSO);
                cmd->SetGraphicsRootDescriptorTable(
                    m_MeshRootSignatureLayout.GetSlot("Sampler"), m_Sampler.GetHandleGPU());
            }
            else
            {
                cmd->SetGraphicsRootSignature(m_MeshRootSignatureLayoutBindless.GetRootSignature());
                cmd->SetPipelineState(m_pMeshPSOBindless);
            }

            //_objects.Submit(&m_RenderQueue);
            //m_RenderQueue.Execute(cmd, m_RenderMode);
            _objects.FlushPendingRemoves();
        });

    // ポストエフェクトを実行
    m_PostProcessStack.Execute(m_RenderGraph, sceneColorHandle, colorHandle);

    m_RenderGraph.AddPass("ModelMeshletPass",
        [colorHandle, depthHandle](RG::PassBuilder& b)
        {
            b.Use(colorHandle, D3D12_RESOURCE_STATE_RENDER_TARGET);
            b.Use(depthHandle, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        },
        [this, colorHandle, depthHandle, &_camera](ID3D12GraphicsCommandList* cmd, const RG::ResourceRegistry& res)
        {
            const auto frameIndex = m_pDevice->GetFrameIndex();
            auto handleRTV = m_pDevice->GetColorTarget(frameIndex)->GetHandleRTV()->HandleCPU;
            auto handleDSV = m_pDevice->GetDepthTarget()->GetHandleDSV()->HandleCPU;
            cmd->OMSetRenderTargets(1, &handleRTV, FALSE, &handleDSV);

            cmd->ClearDepthStencilView(
                handleDSV,
                D3D12_CLEAR_FLAG_DEPTH,
                1.0f,
                0,
                0,
                nullptr);

            SetFullViewport(cmd, m_pDevice->GetWidth(), m_pDevice->GetHeight());

            // DIRECTLY_INDEXEDフラグを使うので、RootSignatureセット前にヒープをセットする
            ID3D12DescriptorHeap* heaps[] =
            {
                m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES)->GetHeap(),
                m_pDevice->GetPool(RHI::Device::POOL_TYPE_SMP)->GetHeap(),
            };
            cmd->SetDescriptorHeaps(_countof(heaps), heaps);

            cmd->SetGraphicsRootSignature(m_ModelMeshletRootSignatureLayout.GetRootSignature());
            cmd->SetPipelineState(m_pModelMeshletPSO);

            auto* pCB = m_ModelMeshletTransformCBs[frameIndex].get();
            auto* pTransform = pCB->GetPtr<TransformCB>();
            pTransform->World = DirectX::XMMatrixIdentity();
            pTransform->View = _camera.GetView();
            pTransform->Proj = _camera.GetProj();

            cmd->SetGraphicsRootConstantBufferView(
                m_ModelMeshletRootSignatureLayout.GetSlot("Transform"), pCB->GetAddress());

            // DebugModeは全メッシュ共通なので1回だけセット
            cmd->SetGraphicsRoot32BitConstant(
                m_ModelMeshletRootSignatureLayout.GetSlot("DebugMode"), m_MeshletDebugMode, 0);

            for (auto& entry : m_ModelMeshletMeshes)
            {
                // メッシュごとにテクスチャIndexをセット
                cmd->SetGraphicsRoot32BitConstant(
                    m_ModelMeshletRootSignatureLayout.GetSlot("TextureIndex"), entry.DiffuseTextureIndex, 0);

                entry.Mesh->Draw(cmd);
            }
        });

    // RenderGraph本体実行
    m_RenderGraph.Execute(_pCmd, pTracker);
}
    
// -------------------------------------------------------------------------------
//      ポストエフェクトの追加
// -------------------------------------------------------------------------------
void SceneRenderer::AddPostProcessEffect(std::unique_ptr<IPostProcessEffect> _effect)
{
    m_PostProcessStack.AddEffect(std::move(_effect));
}


void SceneRenderer::SetFullViewport(ID3D12GraphicsCommandList* _pCmd, uint32_t _width, uint32_t _height)
{
    D3D12_VIEWPORT vp = {};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = static_cast<float>(_width);
    vp.Height = static_cast<float>(_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    D3D12_RECT      scissor = { 0,0,static_cast<LONG>(_width), static_cast<LONG>(_height) };
    _pCmd->RSSetViewports(1, &vp);
    _pCmd->RSSetScissorRects(1, &scissor);
}
