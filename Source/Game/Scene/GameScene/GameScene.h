#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Engine/Scene/IScene.h"
#include <Engine/Camera/FPSCamera/FPSCamera.h>
#include <Engine/Sampler/Sampler.h>
#include <Engine/Mesh/Mesh/Mesh.h>
#include <Engine/Mesh/Material/Material.h>
#include <Engine/Buffer/ConstantBuffer/ConstantBuffer.h>
#include <Engine/GameObject/GameObject.h>
#include <Engine/GameObject/GameObjectManager.h>
#include <Engine/GameObject/Components/MeshComponent/MeshComponent.h>
#include <Engine/GameObject/Components/TransformComponent/TransformComponent.h>
#include <Engine/Mesh/ResData.h>
#include <Engine/RootSignature/RootSignatureLayout/RootSignatureLayout.h>
#include <Engine/Renderer/RenderQueue/RenderQueue.h>

// -------------------------------------------------------------------------------
// GameScene クラス
//
// 概要:
//   通常のゲームシーン。IScene を実装する。
//
//   内部構造:
//     GameObjectManager    全 GameObject の管理（Update / Draw / 追加・削除）
//     FPSCamera            FPS カメラ（WASD + マウス）
//     Sampler              テクスチャサンプラー（LinearWrap）
//     Mesh[] / Material[]  ロードしたモデルのリソース（GameObjectManager が参照）
//
//   各モデルメッシュに対して GameObject を1つ生成し、
//   TransformComponent と MeshComponent を追加して管理する。
// -------------------------------------------------------------------------------
class GameScene : public IScene
{
public:

    // -------------------------------------------------------------------------------
    // 初期化パラメータ
    // -------------------------------------------------------------------------------
    struct Desc
    {
        std::wstring        ModelPath = L"";
        DirectX::XMFLOAT3   CameraPosition = { 0.0f, 1.0f, -5.0f };
        float               CameraMoveSpeed = 5.0f;
        float               CameraRotSpeed = 0.2f;
        float               CameraFov = 60.0f;
        float               CameraNear = 0.1f;
        float               CameraFar = 10000.0f;
    };

    // -------------------------------------------------------------------------------
    // コンストラクタ / デストラクタ
    // -------------------------------------------------------------------------------
    explicit GameScene(const Desc& _desc);
    ~GameScene() override { OnTerm(); }

    // -------------------------------------------------------------------------------
    // IScene インターフェースの実装
    // -------------------------------------------------------------------------------
    bool OnInit(GraphicsDevice* _pGraphicsDevice)           override;
    void OnTerm()                                           override;
    void OnUpdate(float _deltaTime)                         override;
    void OnRender(ID3D12GraphicsCommandList* _pCmd)         override;

private:

    // -------------------------------------------------------------------------------
    // IScene の protected メソッドの実装
    // -------------------------------------------------------------------------------
    bool InitRootSignature(ID3D12Device* _pDevice) override;
    bool InitPipelineState(ID3D12Device* _pDevice) override;

    // -------------------------------------------------------------------------------
    // シーン固有の初期化
    // -------------------------------------------------------------------------------
    void InitCamera();
    bool InitMeshes();
    bool InitSampler();
    bool InitGameObjects();     // GameObjectManager にオブジェクトを登録

    // -------------------------------------------------------------------------------
    // 毎フレームの処理
    // -------------------------------------------------------------------------------
    void UpdateInput(float _deltaTime);
    void UpdateViewProj();      // 全 MeshComponent にカメラ行列を渡す

    // -------------------------------------------------------------------------------
    // RootParameter スロット番号（RootSignature と対応）
    // -------------------------------------------------------------------------------
    static constexpr uint32_t ROOT_PARAM_CBV_TRANSFORM  = 0; // b0: 変換行列（VS）
    static constexpr uint32_t ROOT_PARAM_CBV_MATERIAL   = 1; // b1: マテリアル（PS）
    static constexpr uint32_t ROOT_PARAM_SRV_DIFFUSE    = 2; // t0: ディフューズ（PS）
    static constexpr uint32_t ROOT_PARAM_SMP            = 3; // s0: サンプラー（PS）

    // -------------------------------------------------------------------------------
    // private variables
    // -------------------------------------------------------------------------------
    Desc                m_Desc;

    // カメラ 
    FPSCamera           m_Camera;

    // サンプラー
    Sampler             m_Sampler;

    // GPU リソース（GameObjectManager の MeshComponent が参照する）
    std::vector<std::unique_ptr<Mesh>>      m_Meshes;
    std::vector<std::unique_ptr<Material>>  m_Materials;

    // GameObject 管理
    GameObjectManager   m_ObjectManager;

    // 描画管理
    RenderQueue         m_RenderQueue;

    // JSONから構築されたRootSignature + スロット対応表
    RootSignatureLayout m_RootSignatureLayout;

    // MeshComponent への参照（UpdateViewProj() で使う）
    // GameObjectManager から毎回 GetComponent() するのではなく
    // 初期化時にキャッシュしておく
    std::vector<MeshComponent*>     m_MeshComponents;
};
