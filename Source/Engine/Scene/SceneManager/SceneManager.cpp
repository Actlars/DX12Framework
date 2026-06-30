// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "SceneManager.h"
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
SceneManager::SceneManager()
{ /* DO_NOGHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
SceneManager::~SceneManager() 
{ Term(); }

// -------------------------------------------------------------------------------
//		初期化
// -------------------------------------------------------------------------------
void SceneManager::Init(GraphicsDevice* _pGraphicsDevice)
{
	assert(_pGraphicsDevice);
	m_pGraphicsDevice = _pGraphicsDevice;
}

// -------------------------------------------------------------------------------
//		終了処理
// -------------------------------------------------------------------------------
void SceneManager::Term()
{
	// 予約済みのシーンがあれば破棄する
	m_pNextScene.reset();

	// 現在のシーンを終了させる
	if (m_pCurrentScene != nullptr)
	{
		m_pCurrentScene->OnTerm();
		m_pCurrentScene.reset();
	}

	m_pGraphicsDevice = nullptr;
}

// -------------------------------------------------------------------------------
//		次のシーンを予約する
// -------------------------------------------------------------------------------
void SceneManager::ChangeScene(std::unique_ptr<IScene> _pScene)
{
	if (_pScene == nullptr)
	{
		ELOG("SceneManager::ChangeScene() _pScene is nullptr");
		return;
	}

	// すでに予約済みの場合は上書きする
	// 連続してChangeSceneが呼ばれた場合最後のものを優先する
	if (m_pNextScene != nullptr)
	{
		DLOG("SceneManager::ChangeScene() Overwriting pending scene");
	}

	m_pNextScene = std::move(_pScene);
}

// -------------------------------------------------------------------------------
//		現在のシーンを更新する
// -------------------------------------------------------------------------------
void SceneManager::Update(float _deltaTime)
{
	if (m_pCurrentScene == nullptr) 
	{ return; }

	m_pCurrentScene->OnUpdate(_deltaTime);
}

// -------------------------------------------------------------------------------
//		現在のシーンの描画コマンドを積む
// -------------------------------------------------------------------------------
void SceneManager::Render(ID3D12GraphicsCommandList* _pCmd)
{
	if (m_pCurrentScene == nullptr || _pCmd == nullptr) 
	{ return; }

	m_pCurrentScene->OnRender(_pCmd);
}

// -------------------------------------------------------------------------------
//		シーン切り替え処理（遅延切り替え）
// -------------------------------------------------------------------------------
void SceneManager::ProcessSceneChange()
{
	// 予約がなければ何もしない
	if (m_pNextScene == nullptr) 
	{ return; }

	// 現在のシーンを終了
	if (m_pCurrentScene != nullptr)
	{
		m_pCurrentScene->OnTerm();

		// GPUが現在のリソースを使い終わるまで待つ
		m_pGraphicsDevice->WaitForGPU();

		m_pCurrentScene.reset();
	}

	// 次のシーンを初期化
	if (!m_pNextScene->OnInit(m_pGraphicsDevice))
	{
		ELOG("SceneManager::ProcessSceneChange OnInit failed");
		m_pNextScene.reset();
		return;
	}

	// シーンを入れ替える
	m_pCurrentScene = std::move(m_pNextScene);
	// m_pNextScene はmove後にnullptrになる

	DLOG("SceneManager::Scene Changed successfully");
}

IScene* SceneManager::GetCurrentScene()const
{ return m_pCurrentScene.get(); }

bool SceneManager::HasPendingScene() const
{ return m_pNextScene != nullptr; }
