// -------------------------------------------------------------------------------
//		Includes
// -------------------------------------------------------------------------------
#include "SceneRenderer.h"
#include <Engine/RHI/Core/Device/Device.h>
#include <Engine/GameObject/GameObjectManager.h>
#include <Engine/Camera/FPSCamera/FPSCamera.h>
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

    /*if (!m_MeshBindlessRootSignatureLayout.LoadFromJson(pDevice, L"Assetss/Config/Json/RootSignature/MeshShaderBindless.json"))
    {
        ELOG("SceneRenderer::Init() : MeshRootSignatureLayout MeshShaderBindless load failed");
        return false;
    }

    m_pMeshBindlessPSO = m_pDevice->GetPipelineCache()->GetOrCreate(
        pDevice, L"Assets/Config/Json/PipelinState/MeshShaderBindless.json",
        m_MeshBindlessRootSignatureLayout.GetRootSignature(),
        ResMeshVertex::InputLayout);

    if (m_pMeshBindlessPSO == nullptr)
    {
        ELOG("InitPipelineState() failed");
        return false;
    }*/

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

            cmd->SetGraphicsRootSignature(m_MeshRootSignatureLayout.GetRootSignature());
            cmd->SetPipelineState(m_pMeshPSO);

            ID3D12DescriptorHeap* heaps[] =
            {
                m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES)->GetHeap(),
                m_pDevice->GetPool(RHI::Device::POOL_TYPE_SMP)->GetHeap(),
            };

            cmd->SetDescriptorHeaps(_countof(heaps), heaps);

            cmd->SetGraphicsRootDescriptorTable(
                m_MeshRootSignatureLayout.GetSlot("Sampler"), m_Sampler.GetHandleGPU());

            _objects.Submit(&m_RenderQueue);

            m_RenderQueue.Execute(cmd);

            _objects.FlushPendingRemoves();
        });

    // SceneRenderer::Render() の末尾、m_RenderGraph.Execute()の直前に一時追加
// ── Bindlessテスト（動作確認用、確認後は削除する） ──
    static RHI::RootSignatureLayout s_BindlessTestRS; // static: 毎フレーム再ロードしないための簡易措置
    static bool s_BindlessTestInitialized = false;

    if (!s_BindlessTestInitialized)
    {
        s_BindlessTestRS.LoadFromJson(m_pDevice->GetDevice(), L"Assets/Config/Json/RootSignature/MeshShaderBindless.json");
        s_pBindlessTestPSO = m_pDevice->GetPipelineCache()->GetOrCreate(
            m_pDevice->GetDevice(), L"Assets/Config/Json/PipelineState/BindlessTest.json",
            s_BindlessTestRS.GetRootSignature(), D3D12_INPUT_LAYOUT_DESC{ nullptr, 0 });
        s_BindlessTestInitialized = true;
    }

    // テストに使うインデックス（今回はSceneColorのSRVインデックスをそのまま使う）
    auto* pSceneColorSRV = m_RenderGraph.GetRegistry().GetSRV(sceneColorHandle);


    *testIndexCB.GetPtr<uint32_t>() = pSceneColorSRV->Index; // ← ここがBindlessの核心。IndexをそのままCBVに渡すだけ

    m_RenderGraph.AddPass("BindlessTestPS",
        [colorHandle](RG::PassBuilder& b) { b.Use(colorHandle, D3D12_RESOURCE_STATE_RENDER_TARGET); },
        [this, colorHandle](ID3D12GraphicsCommandList* cmd, const RG::ResourceRegistry& res)
        {
            const auto frameIndex = m_pDevice->GetFrameIndex();
            auto handleRTV = m_pDevice->GetColorTarget(frameIndex)->GetHandleRTV()->HandleCPU;
            cmd->OMSetRenderTargets(1, &handleRTV, FALSE, nullptr);

            SetFullViewport(cmd, m_pDevice->GetWidth(), m_pDevice->GetHeight());

            cmd->SetGraphicsRootSignature(s_BindlessTestRS.GetRootSignature());
            cmd->SetPipelineState(s_pBindlessTestPSO);

            ID3D12DescriptorHeap* heaps[] = { m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES)->GetHeap() };
            cmd->SetDescriptorHeaps(_countof(heaps), heaps);

            cmd->SetGraphicsRootConstantBufferView(0, testIndexCB.GetAddress());

            cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cmd->DrawInstanced(3, 1, 0, 0);
        });

    // ポストエフェクトを実行
    m_PostProcessStack.Execute(m_RenderGraph, sceneColorHandle, colorHandle);

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
    D3D12_VIEWPORT  vp = { 0.0f,0.0f,static_cast<float>(_width), static_cast<float>(_height) };
    D3D12_RECT      scissor = { 0,0,static_cast<LONG>(_width), static_cast<LONG>(_height) };
    _pCmd->RSSetViewports(1, &vp);
    _pCmd->RSSetScissorRects(1, &scissor);
}
