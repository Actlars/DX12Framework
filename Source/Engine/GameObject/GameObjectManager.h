#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/GameObject/GameObject.h>

// -------------------------------------------------------------------------------
// GameObjectManager class
// 
// 概要 : 
//	シーン上の全オブジェクトのライフサイクルを管理するクラス
//	GameSceneがこのクラスを持ち、Update/Drawを一括管理する
// 
// 使い方 : 
//	GameObjectManager manager;
// 
//	auto* pPlayer = manager.Add<GameObject>("Player");
//	pPlayer->AddComponent<TransformComponent>();
//	pPlayer->AddComponent<MeshComponent>();
// 
//	毎フレーム
//	manager.Update(deltaTime);
//	manager.Draw(pCmd);
// 
//	削除
//	manager.Remove(pPlayer);
//	manager.FlushPendingRemoves();	フレーム末に実際に削除
// -------------------------------------------------------------------------------
class GameObjectManager
{
public:

	// -------------------------------------------------------------------------------
	// コンストラクタ
	// -------------------------------------------------------------------------------
	GameObjectManager();

	// -------------------------------------------------------------------------------
	// デストラクタ
	// -------------------------------------------------------------------------------
	~GameObjectManager();

	// -------------------------------------------------------------------------------
	// @brief	GameObjectを生成して追加する
	// 
	//	テンプレートで派生クラスも追加できる
	//	生ポインタを返すが所有権はManagerが持つ
	// 
	// @param[in]	_name	オブジェクト名
	// @return	生成したGameObjectへのポインタ（所有権なし）
	// 
	// 使い方 : 
	//	auto* pObj		= manager.Add<GameObject>("Player");
	//	auto* pEnemy	= manager.Add<GameObject>("Enemy");
	// -------------------------------------------------------------------------------
	template<typename T = GameObject>
	T* Add(const std::string& _name = "GameObject")
	{
		static_assert(std::is_base_of_v<GameObject, T>, "T must derive from GameObject");

		auto obj = std::make_unique<T>(_name);
		auto* pObj = obj.get();

		// mutexで保護
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_Objects.emplace_back(std::move(obj));
		}
		return pObj;
	}

	// -------------------------------------------------------------------------------
	// @brief   既存の unique_ptr<GameObject> を追加する
	//
	// @param[in]   _pObject    追加する GameObject（所有権を移転）
	// @return 追加した GameObject へのポインタ（所有権なし）
	// -------------------------------------------------------------------------------
	template<typename T = GameObject>
	T* AddExisting(std::unique_ptr<T> _pObject)
	{
		static_assert(std::is_base_of_v<GameObject, T>,"T must derive from GameObject");

		auto* pObj = _pObject.get();

		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Objects.emplace_back(std::move(_pObject));

		return pObj;
	}

	// -------------------------------------------------------------------------------
	// @brief	GameObjectを削除する（遅延削除）
	// 
	//	フレーム中にRemoveを呼んでも Update / Draw ループ中に
	//	リストが変更されないように実際の削除はFlushPendingRemoves()でまとめて行う
	// 
	// @param[in]	_pObject	削除するGameObjectへのポインタ
	// -------------------------------------------------------------------------------
	void Remove(GameObject* _pObject);

	// -------------------------------------------------------------------------------
	// @brief	削除予約されたオブジェクトを実際に消す
	// -------------------------------------------------------------------------------
	void FlushPendingRemoves();

	// -------------------------------------------------------------------------------
	// @brief	全ObjectのUpdateを呼ぶ
	// 
	// @param[in]	_deltaTime	前フレームからの経過時間
	// -------------------------------------------------------------------------------
	void Update(float _deltaTime);

	// -------------------------------------------------------------------------------
	// @brief	IRenderableを持つGameObjectのDrawを呼ぶ
	// 
	// @param[in]	_pCmd	記録中のコマンドリスト
	// -------------------------------------------------------------------------------
	void Draw(ID3D12GraphicsCommandList* _pCmd);

	// -------------------------------------------------------------------------------
	// @brief	名前でGameObjectを検索する
	//			同名が複数見つかった場合最初に見つかったものを返す
	// 
	// @param[in]	_name	検索する名前
	// @return	見つかったGameObjectのポインタを返す
	// -------------------------------------------------------------------------------
	GameObject* FindObject(const std::string& _name) const;

	// -------------------------------------------------------------------------------
	// @brief	条件に合う全GameObjectを取得する
	// 
	// @param[in]	_pred	条件関数
	// @return	条件に合うGameObjectのポインタリスト
	// -------------------------------------------------------------------------------
	std::vector<GameObject*> FindAll(const std::function<bool(const GameObject&)>& _pred) const;

	// -------------------------------------------------------------------------------
	// @brief	全GameObjectを削除する
	// -------------------------------------------------------------------------------
	void Clear();

	// -------------------------------------------------------------------------------
	// @brief	管理しているGameObjectの数を返す
	// -------------------------------------------------------------------------------
	size_t ObjectCount() const;

private:

	// -------------------------------------------------------------------------------
	// private variables
	// -------------------------------------------------------------------------------

	// 全GameObject（所有権を持つ）
	std::vector<std::unique_ptr<GameObject>>	m_Objects;

	// 削除予約リスト（フレーム末にFlushPendingRemovesで削除）
	std::vector<GameObject*>					m_PendingRemoves;

	// Add Remove をスレッドセーフにするためのMutex
	mutable std::mutex							m_Mutex;

	GameObjectManager	(const GameObjectManager&) = delete;
	void operator =		(const GameObjectManager&) = delete;
};

