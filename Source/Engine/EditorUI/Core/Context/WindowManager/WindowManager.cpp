#include "WindowManager.h"

// -------------------------------------------------------------------------------
// フレームの開始時に行う処理
// -------------------------------------------------------------------------------
void EditorUI::WindowManager::BeginFrame()
{
	for (auto& [id, state] : m_States)
	{
		(void)id;
		state.Active = false;
	}
}

// -------------------------------------------------------------------------------
// @brief	永続Windowの中からIdを持っているものの有無を返す
// -------------------------------------------------------------------------------
bool EditorUI::WindowManager::Contains(EditorUI::Id _id) const
{
	return m_States.find(_id) != m_States.end();
}

// -------------------------------------------------------------------------------
// 指定したIdに対応するWindowStateの生成Or取得を行う
// -------------------------------------------------------------------------------
EditorUI::WindowState& EditorUI::WindowManager::GetOrCreate(EditorUI::Id _id)
{
	auto it = m_States.find(_id);
	if (it != m_States.end())
	{
		return it->second;	// 既存のものがあれば返す
	}

	// emplaceはキーと値をその場で構築して挿入する操作
	// 戻り値はpair<iterator,bool>で、構造化束縛で受け取る
	// insertedIt : 挿入された要素へのiterator,inserted : 実際に挿入されたか
	auto [insertedIt, inserted] = m_States.emplace(_id, WindowState{});
	insertedIt->second.WindowId = _id;
	m_Order.push_back(_id);	// 新規ウィンドウは背面から開始
	return insertedIt->second;
}

// -------------------------------------------------------------------------------
// 指定したIdのWindowがあれば返す
// -------------------------------------------------------------------------------
EditorUI::WindowState* EditorUI::WindowManager::Find(EditorUI::Id _id)
{
	auto it = m_States.find(_id);
	return it == m_States.end() ? nullptr : &it->second;
}

const EditorUI::WindowState* EditorUI::WindowManager::Find(EditorUI::Id _id) const
{
	auto it = m_States.find(_id);
	return it == m_States.end() ? nullptr : &it->second;
}

// -------------------------------------------------------------------------------
// 指定したIdのWindowの有効化
// -------------------------------------------------------------------------------
void EditorUI::WindowManager::MarkActive(EditorUI::Id _id)
{
	if (WindowState* state = Find(_id))
	{
		state->Active = true;
	}
}

// -------------------------------------------------------------------------------
// Windowの有効化済みかどうかを返す
// -------------------------------------------------------------------------------
bool EditorUI::WindowManager::IsActive(EditorUI::Id _id) const
{
	const WindowState* state = Find(_id);
	return state != nullptr && state->Active == true;
}

// -------------------------------------------------------------------------------
// Windowを安全に削除予約する
// -------------------------------------------------------------------------------
void EditorUI::WindowManager::RequestDestroy(EditorUI::Id _id)
{
	// 無効なIdをはじく
	if (_id == 0) 
	{ return; }

	// すでに削除待ちリストに入っていないかを確認
	auto it = std::find(m_PendingDestroy.begin(), m_PendingDestroy.end(), _id);
	if (it == m_PendingDestroy.end())
	{
		m_PendingDestroy.push_back(_id);
	}
}

std::vector<EditorUI::Id> EditorUI::WindowManager::ConsumePendingDestroy()
{
	std::vector<EditorUI::Id> result;
	result.swap(m_PendingDestroy);
	return result;
}

void EditorUI::WindowManager::DestroyWindowImmediate(EditorUI::Id _id)
{
	if (_id == 0) 
	{ return; }

	m_States.erase(_id);
	m_Order.erase(std::remove(m_Order.begin(), m_Order.end(), _id), m_Order.end());

	if (_id == m_FocusedWindow)
	{
		m_FocusedWindow = 0;
	}

	if (_id == m_HoveredWindow)
	{
		m_HoveredWindow = 0;
	}
}

// -------------------------------------------------------------------------------
// 指定したWindowのIdを、z-order配列の末尾に追加する
// -------------------------------------------------------------------------------
void EditorUI::WindowManager::BringToFron(EditorUI::Id _id)
{
	auto it = std::find(m_Order.begin(), m_Order.end(), _id);

	// it != end() : 見つかった場合のみ処理を行う
	if (it != m_Order.end() && std::next(it) != m_Order.end())
	{
		m_Order.erase(it);		// 元の位置から取り除く
		m_Order.push_back(_id);	// 末尾につけ直す
	}
}

void EditorUI::WindowManager::SetFocused(EditorUI::Id _id)
{
	m_FocusedWindow = _id;
}

EditorUI::Id EditorUI::WindowManager::GetFocused() const
{
	return m_FocusedWindow;
}

void EditorUI::WindowManager::SetHovered(EditorUI::Id _id)
{
	m_HoveredWindow = _id;
}

EditorUI::Id EditorUI::WindowManager::GetHovered() const
{
	return m_HoveredWindow;
}

const std::vector<EditorUI::Id>& EditorUI::WindowManager::GetWindowOrder() const
{
	return m_Order;
}
