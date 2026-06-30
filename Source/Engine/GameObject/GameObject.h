#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <typeindex>
#include <Engine/GameObject/IRenderable.h>
#include <Engine/GameObject/Component/Component.h>

// -------------------------------------------------------------------------------
// GameObject class
// 
// 概要 : 
//	シーン上の全オブジェクトの基底クラス
//	Componentを追加することで機能を拡張するコンポーネント指向設計
// 
//	描画が必要なコンポーネントはIRenderableを実装し、Drawで描画コマンドを積む
//	描画が不要なコンポーネントはComponentのみを継承する
// 
// マルチスレッド対応 : 
//	UpdateとDrawは明確に分離されている
//	将来GameObjectManagerがUpdateを並列化することになった際
//	各GameObjectのUpdateが独立して実行できるようComponent間の参照を避ける
// 
// 使い方 : 
//	auto obj = std::make_unique<GameObject>("Player");
// 
//	コンポーネントの追加
//	auto* pTransform = obj->AddComponent<TansformComponent>();
//	pTransform->SetPosition({0.0f, 0.0f, 0.0f});
// 
//	auto* pMesh = obj->AddComponent<Mesh>();
//	pMesh->SetMesh(pMesh, pMaterial);
// 
//	毎フレーム
//	obj->Update(deltaTime);
//	obj->Draw(pCmd);
// -------------------------------------------------------------------------------
class GameObject
{
public:

	// -------------------------------------------------------------------------------
	// コンストラクタ
	// -------------------------------------------------------------------------------
	explicit GameObject(const std::string& _name = "GameObject");

	// -------------------------------------------------------------------------------
	// デストラクタ
	// -------------------------------------------------------------------------------
	~GameObject();

	// -------------------------------------------------------------------------------
	// @brief	毎フレームの更新処理
	//			アクティブな全ComponentのUpdateを呼ぶ
	// 
	// @param[in]	_deltaTime	前フレームからの経過時間
	// -------------------------------------------------------------------------------
	virtual void Update(float _deltaTime);

	// -------------------------------------------------------------------------------
	// @brief	毎フレームの描画処理
	//			IRenderableを実装しているアクティブなComponentのDrawを呼ぶ
	//			描画コマンドを積むだけで実際のGPU実行はEndFrame後に行っている
	// 
	// @param[in]	_pCmd	記録中のコマンドリスト
	// -------------------------------------------------------------------------------
	virtual void Draw(ID3D12GraphicsCommandList* _pCmd);

	// -------------------------------------------------------------------------------
	// @brief	コンポーネントを追加する
	// 
	//	テンプレートで型を指定して生成・登録する
	//	同じ型は1つまで（重複追加は既存のものを返す）
	//	AddComponent後にComponent::Attachが自動で呼ばれる
	// 
	// @return	追加したコンポーネントへのポインタ
	// 
	// 使い方 : 
	//	auto* pTransform = obj->AddComponent<TransformComponent>();
	// -------------------------------------------------------------------------------
	template<typename T>
	T* AddComponent()
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

		// すでに同じ型のコンポーネントがある場合それを返す
		auto* existing = GetComponent<T>();
		if (existing != nullptr) 
		{ return existing; }

		// コンポーネントを生成して登録
		auto comp = std::make_unique<T>();
		comp->SetOwner(this);
		comp->OnAttach();

		auto* pComp = comp.get();
		const auto typeIdx = std::type_index(typeid(T));
		m_ComponentMap[typeIdx] = pComp;
		m_Components.emplace_back(std::move(comp));

		// IRenderableを実装していれば描画リストにも追加
		if (auto* pRenderable = dynamic_cast<IRenderable*>(pComp)) 
		{ m_Renderables.emplace_back(pRenderable); }

		return pComp;
	}

	// -------------------------------------------------------------------------------
	// @brief	コンポーネントを取得する
	// 
	// @return	見つかったコンポーネントへのポインタ。なければnullptr
	// 
	// 使い方 : 
	//	auto* pTransform = obj->GetComponent<TransformComponent>();
	//	if(pTransform) { pTransform->SetPosition(...); }
	// -------------------------------------------------------------------------------
	template<typename T>
	T* GetComponent() const
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

		const auto typeIdx = std::type_index(typeid(T));
		auto it = m_ComponentMap.find(typeIdx);
		if (it == m_ComponentMap.end()) 
		{ return nullptr; }

		return static_cast<T*>(it->second);
	}

	// -------------------------------------------------------------------------------
	// @brief	コンポーネントを削除する
	// 
	// @return	true	削除成功
	// @return	false	見つからなかった
	// -------------------------------------------------------------------------------
	template<typename T>
	bool RemoveComponent()
	{
		static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");

		const auto typeIdx = std::type_index(typeid(T));
		auto mapIt = m_ComponentMap.find(typeIdx);
		if (mapIt == m_ComponentMap.end()) 
		{ return false; }

		auto* pComp = mapIt->second;
		pComp->OnDetach();

		// 描画リストから削除
		if (auto* pRenderable = dynamic_cast<IRenderable*>(pComp))
		{
			m_Renderables.erase(
				std::remove(m_Renderables.begin(), m_Renderables.end(), pRenderable), 
				m_Renderables.end());
		}

		// コンポーネントリストから削除
		m_Components.erase(
			std::remove_if(m_Components.begin(), m_Components.end(), [pComp](const std::unique_ptr<Component>& c)
				{return c.get() == pComp; }), m_Components.end());
		m_ComponentMap.erase(mapIt);
		return true;
	}

	// @brief	オブジェクト名を返す
	const std::string& GetName() const;

	// @brief	オブジェクト名を設定する
	void SetName(const std::string& _name);

	// @brief	ユニークIDを返す（マルチスレッド識別用）
	uint64_t GetID() const;

	// @brief	アクティブかどうかを返す
	bool IsActive() const { return m_IsActive; }

	// @brief	アクティブ状態を設定する
	void SetActive(bool _active) { m_IsActive = _active; }

protected:

	// -------------------------------------------------------------------------------
	// private variables
	// -------------------------------------------------------------------------------
	std::string m_Name;				// オブジェクト名
	uint64_t	m_ID;				// ユニークID（生成時に自動採番）
	bool		m_IsActive = true;

	// コンポーネントリスト（所有権を持つ）
	std::vector<std::unique_ptr<Component>>	m_Components;

	// 型 → Component* のマップ（GetComponent<T>()の高速検索用）
	std::unordered_map<std::type_index, Component*> m_ComponentMap;

	// IRenderableを実装するコンポーネントへの参照リスト（所有権なし）
	// Drawでここだけをループすることで非描画コンポーネントをスキップする
	std::vector<IRenderable*>						m_Renderables;

private:

	// -------------------------------------------------------------------------------
	// @brief	ユニークIDを生成する（スレッドセーフ）
	// -------------------------------------------------------------------------------
	static uint64_t GenerateID();

	GameObject		(const GameObject&) = delete;
	void operator = (const GameObject&) = delete;
};
