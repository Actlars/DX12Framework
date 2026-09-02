// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "GameScene.h"

#include <Engine/GameObject/GameObject.h>
#include <Engine/GameObject/Components/TransformComponent/TransformComponent.h>
#include <Engine/GameObject/Components/MeshComponent/MeshComponent.h>
#include <Engine/GameObject/Components/MeshletComponent/MeshletComponent.h>
#include <Engine/Input/InputManager/InputManager.h>
#include <Engine/Renderer/PostProcess/PostProcessEffects/BloomEffect/BloomEffect.h>
#include <Engine/Resource/ModelLibrary/ModelLibrary.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
// コンストラクタ
// -------------------------------------------------------------------------------
GameScene::GameScene(const Desc& _desc)
    : m_Desc(_desc)
{ /* DO_NOTHING */
}

// -------------------------------------------------------------------------------
// 初期化
//
// 順序に意味があるのは次の2点
//  1. SceneRenderer を先に作る（RootSignatureのスロット番号がここで決まる）
//  2. カメラ → オブジェクト の順（オブジェクトの配置はカメラに依存しないが、
//     失敗時にどこまで進んだかを追いやすいよう、重い処理を後ろに置く）
// -------------------------------------------------------------------------------
bool GameScene::OnInit(RHI::Device* _pDevice, ModelLibrary* _pModels)
{
    if (_pDevice == nullptr)
    {
        ELOG("GameScene::OnInit() device is null");
        return false;
    }

    m_pDevice       = _pDevice;
    m_pModels       = _pModels;
    m_pInputManager = m_Desc.pInputManager;

    m_SceneRenderer.AddPostProcessEffect(std::make_unique<BloomEffect>());

    if (!m_SceneRenderer.Init(m_pDevice))
    {
        ELOG("GameScene::OnInit() SceneRenderer::Init failed");
        return false;
    }

    InitCamera();

    if (!InitDefaultObjects())
    {
        ELOG("GameScene::OnInit() InitDefaultObjects failed");
        return false;
    }

    m_IsInitialized = true;
    return true;
}

// -------------------------------------------------------------------------------
// 終了処理
//
// GPUリソースを参照しているオブジェクトを先に捨てる
// モデルの実体はModelLibraryが持つため、ここでは解放しない
// -------------------------------------------------------------------------------
void GameScene::OnTerm()
{
    ShowCursor(TRUE);

    m_ObjectManager.Clear();

    m_pModels       = nullptr;
    m_pDevice       = nullptr;
    m_IsInitialized = false;
}

// -------------------------------------------------------------------------------
// 毎フレームの更新処理
// -------------------------------------------------------------------------------
void GameScene::OnUpdate(float _deltaTime)
{
    if (m_pInputManager != nullptr)
    {
        if (m_pInputManager->GetKeyboardInput().IsPressed('M'))
        {
            m_SceneRenderer.ToggleMeshletDebugMode();
        }

        if (m_pInputManager->IsCameraControlActive())
        {
            UpdateInput(_deltaTime);
        }
    }

    // 描画コンポーネントに実体とカメラ行列を結び付ける
    // エディタから増減した直後のコンポーネントも、ここで描ける状態になる
    SyncRenderComponents();

    m_ObjectManager.Update(_deltaTime);

    // 削除予約されたオブジェクトを、ここで実際に取り除く
    // Update / Draw のループ中にリストが変化しない、安全な位置で行う
    m_ObjectManager.FlushPendingRemoves();
}

// -------------------------------------------------------------------------------
// シーンの描画先の設定
//
// 描画先の指定をそのままSceneRendererへ渡し、同時にカメラの縦横比も合わせる
// ここで射影行列を作り直さないと、パネルの形に合わせて絵が伸びてしまう
// -------------------------------------------------------------------------------
void GameScene::SetSceneOutput(const SceneOutput& _output)
{
    m_SceneRenderer.SetOutputTarget(_output);

    if (!_output.IsValid())
    {
        return;
    }

    m_Camera.SetAspect(_output.GetAspect());
    m_Camera.Update();
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
// 起動時のオブジェクトを置く
//
// ここではGPUリソースに触らない
// 「どのモデルの何番目を描くか」を指定するだけで、
// 実体の結び付けは SyncRenderComponents が次のフレームに行う
// -------------------------------------------------------------------------------
bool GameScene::InitDefaultObjects()
{
    if (m_Desc.ModelPath.empty() || m_pModels == nullptr)
    {
        return true;    // 何も置かないシーンも許す
    }

    // -------------------------------------------------------------------------------
    // モデルを先に読み込み、いくつのメッシュに分かれているかを調べる
    //
    // モデル1ファイルは体・髪・服のように複数のメッシュを含むことが多い
    // MeshComponentが描くのはそのうちの1つなので、メッシュの数だけ置く
    // -------------------------------------------------------------------------------
    const ModelResource* pModel = m_pModels->Load(m_Desc.ModelPath);

    if (pModel == nullptr)
    {
        ELOG("GameScene::InitDefaultObjects() model load failed : %ls", m_Desc.ModelPath.c_str());
        return false;
    }

    const std::string modelKey = pModel->GetKey();

    for (uint32_t part = 0; part < pModel->GetPartCount(); ++part)
    {
        auto* pObject = m_ObjectManager.Add<GameObject>("Mesh_" + std::to_string(part));

        pObject->AddComponent<TransformComponent>();
        pObject->AddComponent<MeshComponent>()->SetModelRequest(modelKey, part);
    }

    // -------------------------------------------------------------------------------
    // メッシュシェーダーで描くほうのオブジェクト
    //
    // 同じモデルを、別の描画経路で描いて比べられるようにしている
    // -------------------------------------------------------------------------------
    auto* pMeshletObject = m_ObjectManager.Add<GameObject>("MeshletModel");

    pMeshletObject->AddComponent<TransformComponent>();
    pMeshletObject->AddComponent<MeshletComponent>()->SetModelRequest(modelKey);

    DLOG("GameScene : placed %zu objects (model parts=%zu)",
        m_ObjectManager.ObjectCount(), pModel->GetPartCount());

    return true;
}

// -------------------------------------------------------------------------------
// 描画コンポーネントに実体と毎フレームの値を結び付ける
// -------------------------------------------------------------------------------
void GameScene::SyncRenderComponents()
{
    for (const auto& object : m_ObjectManager.GetObjects())
    {
        if (object == nullptr)
        {
            continue;
        }

        if (auto* pMesh = object->GetComponent<MeshComponent>())
        {
            SyncMeshComponent(*pMesh);
        }

        if (auto* pMeshlet = object->GetComponent<MeshletComponent>())
        {
            SyncMeshletComponent(*pMeshlet);
        }
    }
}

// -------------------------------------------------------------------------------
// MeshComponent 1つ分
// -------------------------------------------------------------------------------
void GameScene::SyncMeshComponent(MeshComponent& _component)
{
    // -------------------------------------------------------------------------------
    // 1. 定数バッファとスロット番号の用意
    //
    // エディタから追加された直後のコンポーネントは、まだ何も持っていない
    // -------------------------------------------------------------------------------
    if (!_component.IsReady())
    {
        const bool initialized = _component.Init(
            m_pDevice->GetDevice(),
            m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES),
            m_pDevice->GetFrameCount());

        if (!initialized)
        {
            ELOG("GameScene::SyncMeshComponent() MeshComponent::Init failed");
            return;
        }

        _component.SetRootLayout(m_SceneRenderer.GetMeshRootSignatureLayout());
        _component.SetRootLayoutBindless(m_SceneRenderer.GetMeshRootSignatureLayoutBIndless());
    }

    // -------------------------------------------------------------------------------
    // 2. 希望しているモデルと、実際に結び付いているものがずれていたら直す
    //
    // ずれるのは
    //      ・エディタでモデルを選び直した
    //      ・プレファブやシーンから読み込んだ直後
    //  のどちらか
    // -------------------------------------------------------------------------------
    if (_component.NeedsModelUpdate())
    {
        Mesh*     pMesh     = nullptr;
        Material* pMaterial = nullptr;

        if (m_pModels != nullptr && !_component.GetModelKey().empty())
        {
            if (const ModelResource* pModel = m_pModels->Load(_component.GetModelKey()))
            {
                if (const ModelPart* pPart = pModel->GetPart(_component.GetPartIndex()))
                {
                    pMesh     = pPart->pMesh;
                    pMaterial = pPart->pMaterial;
                }
            }
        }

        // 見つからなかった場合もApplyModelは呼ぶ
        // 呼ばないと「ずれたまま」と判断され、毎フレーム読み込みを試み続ける
        _component.ApplyModel(pMesh, pMaterial);
    }

    // -------------------------------------------------------------------------------
    // 3. 毎フレーム変わる値を渡す
    // -------------------------------------------------------------------------------
    _component.SetFrameIndex(m_pDevice->GetFrameIndex());
    _component.SetViewProj(m_Camera.GetView(), m_Camera.GetProj());
}

// -------------------------------------------------------------------------------
// MeshletComponent 1つ分
//
// メッシュレットは読み込み時に分割まで済ませるため、
// モデルの結び付けが「読み直し」になる点だけがMeshComponentと異なる
// -------------------------------------------------------------------------------
void GameScene::SyncMeshletComponent(MeshletComponent& _component)
{
    if (_component.NeedsModelUpdate())
    {
        std::wstring absolutePath;

        if (m_pModels != nullptr && !_component.GetModelKey().empty())
        {
            absolutePath = m_pModels->ToAbsolute(_component.GetModelKey()).wstring();
        }

        if (_component.ApplyModel(m_pDevice, absolutePath))
        {
            // 読み込み直後はスロット番号も付け直す
            _component.SetRootLayout(m_SceneRenderer.GetModelMeshletRootSignatureLayout());
        }
    }

    _component.SetFrameIndex(m_pDevice->GetFrameIndex());
    _component.SetViewProj(m_Camera.GetView(), m_Camera.GetProj());
}

// -------------------------------------------------------------------------------
// カメラ操作
// -------------------------------------------------------------------------------
void GameScene::UpdateInput(float _deltaTime)
{
    if (m_pInputManager == nullptr)
    {
        return;
    }

    const Input::KeyboardInput& keyboard = m_pInputManager->GetKeyboardInput();

    // -------------------------------------------------------------------------------
    // 移動
    // GetAsyncKeyStateはウィンドウの前後関係を見ないため、
    // 他アプリを操作中でもカメラが動いてしまう
    // 入力の窓口をKeyboardInputへ一本化する
    // -------------------------------------------------------------------------------
    if (keyboard.IsDown('W')) { m_Camera.MoveForward(_deltaTime); }
    if (keyboard.IsDown('S')) { m_Camera.MoveBack(_deltaTime);    }
    if (keyboard.IsDown('A')) { m_Camera.MoveLeft(_deltaTime);    }
    if (keyboard.IsDown('D')) { m_Camera.MoveRight(_deltaTime);   }
    if (keyboard.IsDown('E')) { m_Camera.MoveUp(_deltaTime);      }
    if (keyboard.IsDown('Q')) { m_Camera.MoveDown(_deltaTime);    }

    // -------------------------------------------------------------------------------
    // 視点回転
    // MouseInputが相対モードで求めた「中央からのずれ」をそのまま角度へ渡す
    // -------------------------------------------------------------------------------
    const POINT delta = m_pInputManager->GetMouseInput().GetDelta();

    if (delta.x != 0 || delta.y != 0)
    {
        m_Camera.AddYaw(static_cast<float>(delta.x));
        m_Camera.AddPitch(static_cast<float>(delta.y));
    }

    m_Camera.Update();
}
