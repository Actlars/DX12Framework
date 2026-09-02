#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Engine/Scene/IScene.h"
#include <Engine/Camera/FPSCamera/FPSCamera.h>
#include <Engine/GameObject/GameObjectManager.h>
#include <Engine/Renderer/SceneRenderer/SceneRenderer.h>

// -------------------------------------------------------------------------------
// 前方宣言
// -------------------------------------------------------------------------------
namespace Input { class InputManager; }

class MeshComponent;
class MeshletComponent;

// -------------------------------------------------------------------------------
// GameScene クラス
//
// 概要 :
//   通常のゲームシーン。ISceneを実装する
//
//   持っているもの
//     GameObjectManager  シーンに置かれた全オブジェクト（所有）
//     FPSCamera          視点
//     SceneRenderer      このシーンの描画手順
//
//   モデルのGPUリソースは持たない
//     読み込みと所有はModelLibraryが受け持つ
//     シーンは「どのモデルを使うか」をコンポーネントへ伝えるだけ
//
//   このシーンの中心的な役目 : 実体の結び付け（SyncRenderComponents）
//     MeshComponentは「どのモデルの何番目を描きたいか」しか持たない
//     一方で、実際に描くには
//         ・定数バッファ（デバイスが要る）
//         ・RootSignatureのスロット番号（SceneRendererが要る）
//         ・Mesh / Material の実体（ModelLibraryが要る）
//     が必要で、これらを知っているのはシーンだけである
//
//     そこでシーンは毎フレーム、まだ揃っていないコンポーネントを見つけて揃える
//     初期化時に一度だけ行わないのは、
//     エディタから途中でコンポーネントが増減しても追従できるようにするため
// -------------------------------------------------------------------------------
class GameScene : public IScene
{
public:

    // -------------------------------------------------------------------------------
    // 初期化パラメータ
    // -------------------------------------------------------------------------------
    struct Desc
    {
        // 起動時に置いておくモデル（プロジェクトからの相対パス）
        // 空にすると、何も置かれていない状態で始まる
        std::wstring            ModelPath       = L"";

        DirectX::XMFLOAT3       CameraPosition  = { 0.0f, 1.0f, -5.0f };
        Input::InputManager*    pInputManager   = nullptr;
        float                   CameraMoveSpeed = 5.0f;
        float                   CameraRotSpeed  = 0.2f;
        float                   CameraFov       = 60.0f;
        float                   CameraNear      = 0.1f;
        float                   CameraFar       = 10000.0f;
    };

    // -------------------------------------------------------------------------------
    // コンストラクタ / デストラクタ
    // -------------------------------------------------------------------------------
    explicit GameScene(const Desc& _desc);
    ~GameScene() override { OnTerm(); }

    // -------------------------------------------------------------------------------
    // IScene インターフェースの実装
    // -------------------------------------------------------------------------------
    bool OnInit(RHI::Device* _pDevice, ModelLibrary* _pModels)  override;
    void OnTerm()                                               override;
    void OnUpdate(float _deltaTime)                             override;
    void OnRender(ID3D12GraphicsCommandList* _pCmd)             override;

    // -------------------------------------------------------------------------------
    // @brief   シーンの描画先を設定する
    //
    //  描画先をSceneRendererへ渡すと同時に、カメラの縦横比も合わせる
    //  ゲーム画面はエディタのパネルへ描かれるため、
    //  ウィンドウの大きさではなくパネルの大きさが縦横比の基準になる
    // -------------------------------------------------------------------------------
    void SetSceneOutput(const SceneOutput& _output) override;

    // @brief   ヒエラルキー/インスペクタが実シーンを編集するための入り口
    GameObjectManager* GetObjectManager() override { return &m_ObjectManager; }

private:

    // -------------------------------------------------------------------------------
    // 初期化
    // -------------------------------------------------------------------------------
    void InitCamera();

    // @brief   起動時のオブジェクトを置く（Desc::ModelPathが空なら何も置かない）
    bool InitDefaultObjects();

    // -------------------------------------------------------------------------------
    // 毎フレームの処理
    // -------------------------------------------------------------------------------
    void UpdateInput(float _deltaTime);

    // -------------------------------------------------------------------------------
    // @brief   描画コンポーネントに、実体と毎フレームの値を結び付ける
    //
    //  やること
    //      1. 定数バッファがまだ無いものを用意する
    //      2. 希望しているモデルと実際がずれているものを結び直す
    //      3. 全部にカメラ行列とフレーム番号を渡す
    //
    //  毎フレーム全体を見て回るが、1と2は「ずれているものだけ」しか処理しない
    //  オブジェクト数はシーンに置ける範囲なので、走査そのものは軽い
    // -------------------------------------------------------------------------------
    void SyncRenderComponents();

    // 1つ分の面倒を見る。SyncRenderComponentsから呼ぶ
    void SyncMeshComponent(MeshComponent& _component);
    void SyncMeshletComponent(MeshletComponent& _component);

    // -------------------------------------------------------------------------------
    // private variables
    // -------------------------------------------------------------------------------
    Desc                m_Desc;

    // シーンに置かれたオブジェクト（所有）
    GameObjectManager   m_ObjectManager;

    // 視点
    FPSCamera           m_Camera;

    // このシーンの描画手順
    SceneRenderer       m_SceneRenderer;
    PostProcessStack    m_PostProcessStack;

    // 参照だけ（所有権なし）
    ModelLibrary*           m_pModels       = nullptr;
    Input::InputManager*    m_pInputManager = nullptr;
};
