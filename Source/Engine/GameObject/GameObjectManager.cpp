// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "GameObjectManager.h"
#include <Engine/Renderer/RenderQueue/RenderQueue.h>
#include <Engine/Renderer/RenderQueue/MeshletRenderQueue/MeshletRenderQueue.h>

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
GameObjectManager::GameObjectManager()
{ /* DO_NOTHING */ }

GameObjectManager::~GameObjectManager() 
{ Clear(); }

// -------------------------------------------------------------------------------
//		削除予約
// -------------------------------------------------------------------------------
void GameObjectManager::Remove(GameObject* _pObject)
{
	if (_pObject == nullptr) 
	{ return; }

	std::lock_guard<std::mutex> lock(m_Mutex);

	// 重複登録を防ぐ
	auto it = std::find(m_PendingRemoves.begin(), m_PendingRemoves.end(), _pObject);
	if (it == m_PendingRemoves.end()) 
	{ m_PendingRemoves.emplace_back(_pObject); }
}

// -------------------------------------------------------------------------------
//		削除予約を実際に実行する（フレーム末に呼ぶ）
// -------------------------------------------------------------------------------
void GameObjectManager::FlushPendingRemoves()
{
	if (m_PendingRemoves.empty()) 
	{ return; }

	std::lock_guard<std::mutex> lock(m_Mutex);

	for (auto* pObj : m_PendingRemoves)
	{
		m_Objects.erase(
			std::remove_if(m_Objects.begin(), m_Objects.end(),
				[pObj](const std::unique_ptr<GameObject>& obj)
				{return obj.get() == pObj; }),
			m_Objects.end());
	}

	m_PendingRemoves.clear();
}

// -------------------------------------------------------------------------------
//		破棄せずに管理下から切り離す（所有権を呼び出し側へ渡す）
// -------------------------------------------------------------------------------
std::unique_ptr<GameObject> GameObjectManager::Detach(GameObject* _pObject)
{
	if (_pObject == nullptr)
	{ return nullptr; }

	std::lock_guard<std::mutex> lock(m_Mutex);

	auto it = std::find_if(m_Objects.begin(), m_Objects.end(),
		[_pObject](const std::unique_ptr<GameObject>& _obj)
		{ return _obj.get() == _pObject; });

	if (it == m_Objects.end())
	{ return nullptr; }

	// 所有権を手放してから、空になった枠を取り除く
	std::unique_ptr<GameObject> detached = std::move(*it);
	m_Objects.erase(it);

	// 削除予約が残っていると、戻したあとに消されてしまうため取り下げる
	m_PendingRemoves.erase(
		std::remove(m_PendingRemoves.begin(), m_PendingRemoves.end(), _pObject),
		m_PendingRemoves.end());

	return detached;
}

// -------------------------------------------------------------------------------
//		全オブジェクトの更新
// -------------------------------------------------------------------------------
void GameObjectManager::Update(float _deltaTime)
{
	for (auto& obj : m_Objects)
	{
		if (obj->IsActive()) 
		{ obj->Update(_deltaTime); }
	}
}

// -------------------------------------------------------------------------------
//		IRenderableを持つGameObjectのDrawを呼ぶ
// -------------------------------------------------------------------------------
void GameObjectManager::Submit(RenderQueue* _pQueue)
{
	for (auto& obj : m_Objects)
	{
		if (obj->IsActive()) 
		{ obj->Submit(_pQueue); }
	}
}

// -------------------------------------------------------------------------------
//		IMeshletRenderableを持つGameObjectのDrawを呼ぶ
// -------------------------------------------------------------------------------
void GameObjectManager::SubmitMeshlet(MeshletRenderQueue* _pQueue)
{
	for (auto& obj : m_Objects)
	{
		if (obj->IsActive())
		{ obj->SubmitMeshlet(_pQueue); }
	}
}

void GameObjectManager::SubmitDebugLine(DebugLineRenderQueue* _pQueue)
{
	for (auto& obj : m_Objects)
	{
		if (obj->IsActive())
		{ obj->SubmitDebugLine(_pQueue); }
	}
}

// -------------------------------------------------------------------------------
//		名前で検索
// -------------------------------------------------------------------------------
GameObject* GameObjectManager::FindObject(const std::string& _name) const
{
	for (auto& obj : m_Objects)
	{
		if (obj->GetName() == _name) 
		{ return obj.get(); }
	}

	return nullptr;
}

// -------------------------------------------------------------------------------
//		条件で検索
// -------------------------------------------------------------------------------
std::vector<GameObject*> GameObjectManager::FindAll(
	const std::function<bool(const GameObject&)>& _pred) const
{
	std::vector<GameObject*> result;
	for (auto& obj : m_Objects)
	{
		if (_pred(*obj)) 
		{ result.emplace_back(obj.get()); }
	}

	return result;
}

// -------------------------------------------------------------------------------
// 同じオブジェクトを探す
// -------------------------------------------------------------------------------
bool GameObjectManager::Contains(const GameObject* _pObject)
{
	for (const auto& obj : m_Objects)
	{
		if (obj.get() == _pObject)
		{
			return true;
		}
	}
	return false;
}

// -------------------------------------------------------------------------------
//		全削除
// -------------------------------------------------------------------------------
void GameObjectManager::Clear()
{
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_PendingRemoves.clear();
	m_Objects.clear();
}

// -------------------------------------------------------------------------------
//		オブジェクト数を返す
// -------------------------------------------------------------------------------
size_t GameObjectManager::ObjectCount() const
{
	return m_Objects.size();
}
