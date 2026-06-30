// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "GameObject.h"

// -------------------------------------------------------------------------------
//		ユニークIDの生成（スレッドセーフ）
// -------------------------------------------------------------------------------
uint64_t GameObject::GenerateID()
{
	// static atomic でスレッドセーフにインクリメントする
	// 将来マルチスレッドでgameObjectを生成してもIDが衝突しない
	static std::atomic<uint64_t> s_NextID{ 1 };
	return s_NextID.fetch_add(1, std::memory_order_relaxed);
}

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
GameObject::GameObject(const std::string& _name)
: m_Name (_name)
, m_ID(GenerateID())
, m_IsActive(true)
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
GameObject::~GameObject()
{
	// コンポーネントを逆順にOnDetachしてから解放する
	// 追加した順と逆に解放することで依存関係を安全に処理できる
	for (auto it = m_Components.rbegin(); it != m_Components.rend(); ++it) 
	{ (*it)->OnDetach(); }

	m_Renderables.clear();
	m_ComponentMap.clear();
	m_Components.clear();
}

// -------------------------------------------------------------------------------
//		毎フレームの更新処理
// -------------------------------------------------------------------------------
void GameObject::Update(float _deltaTime)
{
	if (!m_IsActive) 
	{ return; }

	for (auto& comp : m_Components)
	{
		// コンポーネントが有効な場合のみUpdateを呼ぶ
		if (comp->IsActive()) 
		{ comp->Update(_deltaTime); }
	}
}

// -------------------------------------------------------------------------------
//		毎フレームの描画処理
// -------------------------------------------------------------------------------
void GameObject::Draw(ID3D12GraphicsCommandList* _pCmd)
{
	if (!m_IsActive || _pCmd == nullptr) 
	{ return; }

	// IRenderableを実装しているComponentだけをループする
	// 非描画コンポーネントは（TransformComponent等）はスキップする
	for (auto* pRenderable : m_Renderables)
	{
		if (pRenderable->IsVisible()) 
		{ pRenderable->Draw(_pCmd); }
	}
}

const std::string& GameObject::GetName() const
{
	return m_Name;
}

void GameObject::SetName(const std::string& _name)
{
	m_Name = _name;
}

uint64_t GameObject::GetID() const
{
	return m_ID;
}