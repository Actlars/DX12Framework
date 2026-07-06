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
bool GameScene::OnInit(RHI::Device* _pDevice)
{
    assert(_pDevice != nullptr);
    m_pDevice = _pDevice;

    auto* pDevice = m_pDevice->GetDevice();

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

    m_pDevice           = nullptr;
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
    auto* pTracker = m_pDevice->GetResourceStateTracker();

    // フレームの最初に、外部リソース（バックバッファ・DepthTarget）をグラフに取り込む
    const auto frameIndex = m_pDevice->GetFrameIndex();
    auto colorHandle = m_RenderGraph.ImportResource(
        "BackBuffer", m_pDevice->GetColorTarget(frameIndex)->GetResource());
    auto depthHandle = m_RenderGraph.ImportResource(
        "DepthBuffer", m_pDevice->GetDepthTarget()->GetResource());

    RG::TransientResourceDesc sceneColorDesc;
    sceneColorDesc.Width = m_pDevice->GetWidth();
    sceneColorDesc.Height = m_pDevice->GetHeight();
    sceneColorDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    sceneColorDesc.ClearColor[0] = sceneColorDesc.ClearColor[1] = sceneColorDesc.ClearColor[2] = 0.0f;
    sceneColorDesc.ClearColor[3] = 1.0f;

    auto sceneColorHandle = m_RenderGraph.GetRegistry().CreateTransient(
        m_pDevice->GetDevice(), "SceneColor", sceneColorDesc,
        m_pDevice->GetTransientResourcePool(),
        m_pDevice->GetPool(RHI::Device::POOL_TYPE_RTV),
        m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES));

    m_RenderGraph.AddPass(
        "MainPass",
        // Setup : このパスが使用するリソースとステートを宣言する
        [sceneColorHandle, depthHandle](RG::PassBuilder& _builder)
        {
            _builder.Use(sceneColorHandle, D3D12_RESOURCE_STATE_RENDER_TARGET);
            _builder.Use(depthHandle, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        },
        // Execute : 実際の描画。ここに来た時点でバリアは解決済み
        [this, sceneColorHandle](ID3D12GraphicsCommandList* cmd, const RG::ResourceRegistry& res)
        {
            auto* pRTV = res.GetRTV(sceneColorHandle);
            auto handleDSV = m_pDevice->GetDepthTarget()->GetHandleDSV()->HandleCPU;
            cmd->OMSetRenderTargets(1, &pRTV->HandleCPU, FALSE, &handleDSV);

            // クリア処理
            const float clearColor[4] = { 0.0f,0.0f,1.0f,1.0f };
            cmd->ClearRenderTargetView(pRTV->HandleCPU, clearColor, 0, nullptr);
            cmd->ClearDepthStencilView(handleDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            cmd->SetGraphicsRootSignature(m_RootSignatureLayout.GetRootSignature());
            cmd->SetPipelineState(m_pPSO);

            ID3D12DescriptorHeap* heaps[] =
            {
                m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES)->GetHeap(),
                m_pDevice->GetPool(RHI::Device::POOL_TYPE_SMP)->GetHeap(),
            };

            cmd->SetDescriptorHeaps(_countof(heaps), heaps);

            cmd->SetGraphicsRootDescriptorTable(
                m_RootSignatureLayout.GetSlot("Sampler"), m_Sampler.GetHandleGPU());

            m_ObjectManager.Submit(&m_RenderQueue);

            m_RenderQueue.Execute(cmd);

            m_ObjectManager.FlushPendingRemoves();
        });

    // ExtractとComposite用のRootSignatureとPSOをキャッシュから取得
    auto* pPostProcessRS = m_PostProcessRootSignatureLayout.GetRootSignature();
    auto* pExtractPSO = m_pDevice->GetPipelineCache()->GetOrCreate(
        m_pDevice->GetDevice(), L"Assets/Config/Json/PipelineState/BloomExtract.json",
        pPostProcessRS, D3D12_INPUT_LAYOUT_DESC{ nullptr,0 });  // 頂点レイアウトなしなのでレイアウトは空
    auto* pCompositePSO = m_pDevice->GetPipelineCache()->GetOrCreate(
        m_pDevice->GetDevice(), L"Assets/Config/Json/PipelineState/BloomComposite.json",
        pPostProcessRS, D3D12_INPUT_LAYOUT_DESC{ nullptr,0 });
    auto* pBlurPSO = m_pDevice->GetPipelineCache()->GetOrCreate(
        m_pDevice->GetDevice(), L"Assets/Config/Json/PipelineState/BloomBlur.json",
        pPostProcessRS, D3D12_INPUT_LAYOUT_DESC{ nullptr,0 });

    // Transientバッファを確保（画面の半分サイズ、ブルーム用）
    RG::TransientResourceDesc bloomDesc;
    bloomDesc.Width = m_pDevice->GetWidth() / 2;
    bloomDesc.Height = m_pDevice->GetHeight() / 2;
    bloomDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    bloomDesc.ClearColor[0] = bloomDesc.ClearColor[1] = bloomDesc.ClearColor[2] = 0.0f;
    bloomDesc.ClearColor[3] = 1.0f;

    // A: Extractの出力先、Blurの入力元として使う
    auto bloomHandle = m_RenderGraph.GetRegistry().CreateTransient(
        m_pDevice->GetDevice(), "BloomBuffer", bloomDesc,
        m_pDevice->GetTransientResourcePool(),
        m_pDevice->GetPool(RHI::Device::POOL_TYPE_RTV),
        m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES));    // RTV + SRV 両方発行

    // B: Blurの出力先、Compositeの入力元として使う
    auto bloomHandleB = m_RenderGraph.GetRegistry().CreateTransient(
        m_pDevice->GetDevice(), "BloomBufferB", bloomDesc,
        m_pDevice->GetTransientResourcePool(),
        m_pDevice->GetPool(RHI::Device::POOL_TYPE_RTV),
        m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES));

    // -------------------------------------------------------------------------------
    // Extractパス : sceneColorHandleを読み、明るい部分だけをBloomHandle（A）に書き込む
    // -------------------------------------------------------------------------------
    m_RenderGraph.AddPass("BloomExtract", [bloomHandle, sceneColorHandle](RG::PassBuilder& b)
        {
            // Setup : このパスが使うリソースと要求ステートを宣言する
            b.Use(sceneColorHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            b.Use(bloomHandle, D3D12_RESOURCE_STATE_RENDER_TARGET);
        },
        // Execute : Setupの宣言に基づき、RenderGraphがバリア解決済みの状態で呼ばれる
        [this,sceneColorHandle, bloomHandle, pExtractPSO](ID3D12GraphicsCommandList* cmd, const RG::ResourceRegistry& res)
        {
            // 描画先をbloomHandle(A)に切り替える
            // DX12はOMSetRenderTargetsを呼びなおさない限り前のパスの設定が残り続けるため、
            // パスが変わるたびに毎回明示する
            auto* pRTV = res.GetRTV(bloomHandle);
            cmd->OMSetRenderTargets(1, &pRTV->HandleCPU, FALSE, nullptr);

            // bloomHandleは画面の半分のサイズで確保する
            // フルサイズのビューポートだと、描画範囲がずれるため、TexelSizeに合わせておく
            SetFullViewport(cmd, m_pDevice->GetWidth() / 2, m_pDevice->GetHeight() / 2);

            cmd->SetGraphicsRootSignature(m_PostProcessRootSignatureLayout.GetRootSignature());
            cmd->SetPipelineState(pExtractPSO);

            ID3D12DescriptorHeap* heaps[] = { m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES)->GetHeap() };
            cmd->SetDescriptorHeaps(_countof(heaps), heaps);

            // MainPassが描いた絵（sceneColorHandle）をSRVとしてシェーダーに渡す
            auto* pSrcSRV = res.GetSRV(sceneColorHandle);
            cmd->SetGraphicsRootDescriptorTable(
                m_PostProcessRootSignatureLayout.GetSlot("SourceTexture"), pSrcSRV->HandleGPU);

            // 頂点バッファ・インデックスバッファなしで、頂点IDだけから
            // 画面全体を覆う三角形を1枚生成する
            cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cmd->DrawInstanced(3, 1, 0, 0);
        });

    // -------------------------------------------------------------------------------
    // Blurパス : bloomHandle(A)に5x5ガウシアンブラーをかけて、bloomHandleB(B)に書く
    // -------------------------------------------------------------------------------
    m_RenderGraph.AddPass("BloomBlur", [bloomHandle, bloomHandleB](RG::PassBuilder& b)
        {
            b.Use(bloomHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); // A : 読む
            b.Use(bloomHandleB, D3D12_RESOURCE_STATE_RENDER_TARGET);        // B : 書く
        },
        [this, bloomHandle, bloomHandleB, pBlurPSO](ID3D12GraphicsCommandList* cmd, const RG::ResourceRegistry& res)
        {
            auto* pRTV = res.GetRTV(bloomHandleB);
            cmd->OMSetRenderTargets(1, &pRTV->HandleCPU, FALSE, nullptr);

            // bloomHandleBもbloomHandleと同じ半分サイズなので、同じビューポートを使う
            SetFullViewport(cmd, m_pDevice->GetWidth() / 2, m_pDevice->GetHeight() / 2);

            cmd->SetGraphicsRootSignature(m_PostProcessRootSignatureLayout.GetRootSignature());
            cmd->SetPipelineState(pBlurPSO);

            ID3D12DescriptorHeap* heaps[] = { m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES)->GetHeap() };
            cmd->SetDescriptorHeaps(_countof(heaps), heaps);

            // ぼかす対象はExtractの結果(A)BlurPS.hlsl側で
            // 5x5ガウシアンカーネルを（重みの合計 = 1.0になるように正規化済み）適用する
            auto* pSrcSRV = res.GetSRV(bloomHandle);
            cmd->SetGraphicsRootDescriptorTable(
                m_PostProcessRootSignatureLayout.GetSlot("SourceTexture"), pSrcSRV->HandleGPU);

            // TexelSize(1 / バッファ幅、1 / バッファ高さ)をシェーダーに渡す
            // これがないと、シェーダー内でサンプリング位置を何ピクセルずらせばいいのかわからない
            cmd->SetGraphicsRootConstantBufferView(
                m_PostProcessRootSignatureLayout.GetSlot("BlurParams"), m_BlurParams->GetAddress());

            cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cmd->DrawInstanced(3, 1, 0, 0);
        });
    
    // -------------------------------------------------------------------------------
    // Compositeパス : sceneColorHandle(元の絵) + bloomHandleB(ぼかし済みブルーム)を
    //                 加算合成し、colorHandle(バックバッファ)に書き込む
    // -------------------------------------------------------------------------------
    m_RenderGraph.AddPass("BloomComposite", [sceneColorHandle, bloomHandleB, colorHandle](RG::PassBuilder& b)
        {
            b.Use(sceneColorHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);    // 元の絵を読む
            b.Use(bloomHandleB, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);        // ぼかし済みを読む
            b.Use(colorHandle, D3D12_RESOURCE_STATE_RENDER_TARGET);                 // バックバッファに書く
        },
        [this, sceneColorHandle, bloomHandleB, colorHandle, pCompositePSO](ID3D12GraphicsCommandList* cmd, const RG::ResourceRegistry& res)
        {
            // colorHandleはImportリソース（バックバッファ）なので、Transientのように
            // ResourceRegistry::GetRTV()では取得できない。GraphicsDevice側から直接取得する
            const auto frameIndex = m_pDevice->GetFrameIndex();
            auto handleRTV = m_pDevice->GetColorTarget(frameIndex)->GetHandleRTV()->HandleCPU;
            cmd->OMSetRenderTargets(1, &handleRTV, FALSE, nullptr);

            // バックバッファはフルサイズなので、直前のBlurパス（半分）から
            // ビューポートをフルサイズに戻す
            SetFullViewport(cmd, m_pDevice->GetWidth(),m_pDevice->GetHeight());

            cmd->SetGraphicsRootSignature(m_PostProcessRootSignatureLayout.GetRootSignature());
            cmd->SetPipelineState(pCompositePSO);

            ID3D12DescriptorHeap* heaps[] = { m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES)->GetHeap() };
            cmd->SetDescriptorHeaps(_countof(heaps), heaps);

            // t0 : 元のシーンの絵
            auto* pSrcSRV = res.GetSRV(sceneColorHandle);
            cmd->SetGraphicsRootDescriptorTable(
                m_PostProcessRootSignatureLayout.GetSlot("SourceTexture"), pSrcSRV->HandleGPU);

            // t1 : ぼかし済みのブルーム結果（Blurパスの出力 = B）
            auto* pBloomSRV = res.GetSRV(bloomHandleB);
            cmd->SetGraphicsRootDescriptorTable(
                m_PostProcessRootSignatureLayout.GetSlot("BloomTexture"), pBloomSRV->HandleGPU);

            cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cmd->DrawInstanced(3, 1, 0, 0);
        });

    m_RenderGraph.Execute(_pCmd, pTracker);
}

// ===============================================================================
// private
// ===============================================================================

// -------------------------------------------------------------------------------
// RootSignature 生成
// -------------------------------------------------------------------------------
bool GameScene::InitRootSignature(ID3D12Device* _pDevice)
{
    if (!m_RootSignatureLayout.LoadFromJson(_pDevice, L"Assets/Config/Json/RootSignature/MeshShader.json"))
    {
        ELOG("RootSignatureLayout::LoadFromJson failed");
        return false;
    }

    if (!m_PostProcessRootSignatureLayout.LoadFromJson(_pDevice, L"Assets/Config/Json/RootSignature/PostProcess.json"))
    {
        return false;
    }
    
    return true;
}

// -------------------------------------------------------------------------------
// PSO 生成
// -------------------------------------------------------------------------------
bool GameScene::InitPipelineState(ID3D12Device* _pDevice)
{
    m_pPSO = m_pDevice->GetPipelineCache()->GetOrCreate(
        _pDevice,
        L"Assets/Config/Json/PipelineState/MeshShader.json",
        m_RootSignatureLayout.GetRootSignature(),
        ResMeshVertex::InputLayout);

    if (m_pPSO == nullptr)
    {
        ELOG("InitPipelineState() failed");
        return false;
    }

    m_BlurParams = std::make_unique<RHI::ConstantBuffer>();
    m_BlurParams->Init(_pDevice, m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES), sizeof(BlurParamsCB));
    auto* pU = m_BlurParams->GetPtr<BlurParamsCB>();
    pU->TexelSize = { 1.0f / (m_pDevice->GetWidth() / 2.0f) , 1.0f / (m_pDevice->GetHeight() / 2.0f)  };

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
        static_cast<float>(m_pDevice->GetWidth()) /
        static_cast<float>(m_pDevice->GetHeight()));
    m_Camera.SetNearFar(m_Desc.CameraNear, m_Desc.CameraFar);
    m_Camera.Update();
}

// -------------------------------------------------------------------------------
// メッシュ・マテリアルのロード
// -------------------------------------------------------------------------------
bool GameScene::InitMeshes()
{
    auto* pDevice   = m_pDevice->GetDevice();
    auto* pQueue    = m_pDevice->GetQueue();
    auto* pPool     = m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES);

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
    auto* pDevice   = m_pDevice->GetDevice();
    auto* pSmpPool  = m_pDevice->GetPool(RHI::Device::POOL_TYPE_SMP);

    if (!m_Sampler.Init(pDevice, pSmpPool, RHI::Sampler::CreateLinearWrap()))
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
    auto* pDevice   = m_pDevice->GetDevice();
    auto* pPool     = m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES);
    const auto fc   = m_pDevice->GetFrameCount();

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
    const auto frameIndex = m_pDevice->GetFrameIndex();
    const auto view = m_Camera.GetView();
    const auto proj = m_Camera.GetProj();

    for (auto* pMeshComp : m_MeshComponents)
    {
        if (pMeshComp == nullptr) { continue; }
        pMeshComp->SetFrameIndex(frameIndex);
        pMeshComp->SetViewProj(view, proj);
    }
}

void GameScene::SetFullViewport(ID3D12GraphicsCommandList* _pCmd, uint32_t _width, uint32_t _height)
{
    D3D12_VIEWPORT vp = { 0.0f,0.0f,static_cast<float>(_width), static_cast<float>(_height) };
    D3D12_RECT scissor = { 0,0,static_cast<LONG>(_width), static_cast<LONG>(_height) };
    _pCmd->RSSetViewports(1, &vp);
    _pCmd->RSSetScissorRects(1, &scissor);
}
