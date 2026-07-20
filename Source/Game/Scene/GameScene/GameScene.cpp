// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "GameScene.h"
#include <Engine/Utility/Debug/Logger/Logger.h>
#include <Engine/Utility/FileUtil/FileUtil.h>
#include <Engine/Mesh/MeshLoader/MeshLoader.h>
#include <d3dcompiler.h>
#include <Engine/Renderer/PostProcess/PostProcessEffects/BloomEffect/BloomEffect.h>

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

    m_SceneRenderer.AddPostProcessEffect(std::make_unique<BloomEffect>());
    if (!m_SceneRenderer.Init(m_pDevice))
    {
        ELOG("SceneRenderer::Init() failed");
        return false;
    }

    InitCamera();

    //if (!InitMeshes())      { ELOG("InitMeshes() failed");      return false; }
    //if (!InitGameObjects()) { ELOG("InitGameObjects() failed"); return false; }
    //if (!InitMeshlets())    { ELOG("InitMeshlets() failed");    return false; }

    if (!m_NTCRunner.Run(m_pDevice))
    {
        ELOG("NTCImageDecodeTestRunner::Run() failed");
        // 失敗してもゲーム自体は続行させたいならreturnしない
    }
    else
    {
        m_SceneRenderer.SetNTCPreviewTexture(&m_NTCRunner.GetBakedTexture());
    }

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
    m_MeshletComponents = nullptr;
    m_ObjectManager.Clear();

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
    m_SceneRenderer.Render(_pCmd, m_ObjectManager, m_Camera);
}

// ===============================================================================
// private
// ===============================================================================


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
        if (!mat->Init(m_pDevice, resMat))
        {
            ELOG("Material::Init() failed."); return false;
        }
        m_Materials.emplace_back(std::move(mat));
    }

    return true;
}

bool GameScene::InitMeshlets()
{
    auto* pObj = m_ObjectManager.Add<GameObject>("MeshletModel");

    auto* pTransform = pObj->AddComponent<TransformComponent>();
    pTransform->SetPosition({ 0.0f,0.0f,0.0f });
    pTransform->SetScale({ 1.0f,1.0f,1.0f });

    auto* pMeshletComp = pObj->AddComponent<MeshletComponent>();
    if (!pMeshletComp->Init(m_pDevice, m_Desc.ModelPath))
    {
        ELOG("MeshletComponent::Init() failed");
        return false;
    }

    pMeshletComp->SetRootLayout(m_SceneRenderer.GetModelMeshletRootSignatureLayout());

    m_MeshletComponents = pMeshletComp;

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
        Material* pMaterial = (materialId < m_Materials.size()) ? m_Materials[materialId].get() : nullptr;

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

        pMeshComp->SetMesh(m_Meshes[i].get(), pMaterial);
        // RootSignature のスロット番号を設定（GameScene の定義と合わせる）
        pMeshComp->SetRootLayout(m_SceneRenderer.GetMeshRootSignatureLayout());
        pMeshComp->SetRootLayoutBindless(m_SceneRenderer.GetMeshRootSignatureLayoutBIndless());

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

    // Mキーでメッシュレット表示切り替え（エッジ検出）
    static bool sPrevMKey = false;
    const bool curMKey = (GetAsyncKeyState('M') & 0x8000) != 0;
    if (curMKey && !sPrevMKey)
    {
        m_SceneRenderer.ToggleMeshletDebugMode();
    }
    sPrevMKey = curMKey;

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

    if (m_MeshletComponents != nullptr)
    {
        m_MeshletComponents->SetFrameIndex(frameIndex);
        m_MeshletComponents->SetViewProj(view, proj);
    }
}
