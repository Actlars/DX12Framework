#include "WidgetInteraction.h"

// -------------------------------------------------------------------------------
// 入力を占有しているウィジェットの生死を判定する
//
// 前フレームに一度もKeepActiveIdAliveが呼ばれなかった＝そのウィジェットが
// 描画されなくなった、ということなので、所有権とテキスト編集状態を解放する
// これがないと、ウィンドウを閉じた瞬間に入力が誰にも届かなくなる
// -------------------------------------------------------------------------------
void EditorUI::WidgetInteraction::NewFrame()
{
	// 前フレームに生存申告されなかったActiveIdは、Widgetが消えたものとして解除する
	if (m_ActiveId != 0 && !m_ActiveIdIsAlive)
	{
		m_ActiveId = 0;
		m_ActiveIdDragAccumulator = 0.0;
	}

	// 編集中のウィジェットごと消えた場合は、確定させる相手がいないので破棄する
	if (m_TextEdit.Widget != 0 && !m_TextEdit.Alive)
	{
		m_TextEdit.Reset();
	}

	// 生存申告はフレームごとに取り直す
	m_ActiveIdIsAlive	= false;
	m_TextEdit.Alive	= false;
	m_HoveredId			= 0;
}

EditorUI::Id EditorUI::WidgetInteraction::GetActiveId() const
{
	return m_ActiveId;
}

bool EditorUI::WidgetInteraction::IsActive(EditorUI::Id _id) const
{
	return m_ActiveId != 0 && m_ActiveId == _id;
}

bool EditorUI::WidgetInteraction::IsAnyActive() const
{
	return m_ActiveId != 0;
}

void EditorUI::WidgetInteraction::SetActive(EditorUI::Id _id, const DirectX::XMFLOAT2& _mousePos)
{
	if (m_ActiveId == _id)
	{
		m_ActiveIdIsAlive = true;
		return;
	}

	m_ActiveId					= _id;
	m_ActiveIdIsAlive			= true;
	m_ActiveIdClickPos			= _mousePos;
	m_ActiveIdDragAccumulator	= 0.0f;
}

void EditorUI::WidgetInteraction::ClearActive(EditorUI::Id _id)
{
	// 別Widgetが既に所有権を持っている場合は横取りして解除しない
	if (m_ActiveId != _id)
	{ return; }

	m_ActiveId					= 0;
	m_ActiveIdDragAccumulator	= 0.0f;
}

void EditorUI::WidgetInteraction::KeepAlive(EditorUI::Id _id)
{
	if (m_ActiveId == _id)
	{
		m_ActiveIdIsAlive = true;
	}
}

EditorUI::Id EditorUI::WidgetInteraction::GetHoveredId() const
{
	return m_HoveredId;
}

bool EditorUI::WidgetInteraction::IsHoveredId(EditorUI::Id _id) const
{
	return m_HoveredId != 0 && m_HoveredId == _id;
}

void EditorUI::WidgetInteraction::SetHoveredId(EditorUI::Id _id)
{
	m_HoveredId = _id;
}

const DirectX::XMFLOAT2& EditorUI::WidgetInteraction::GetActiveClickPos() const
{
	return m_ActiveIdClickPos;
}

double& EditorUI::WidgetInteraction::GetDragAccumulator()
{
	return m_ActiveIdDragAccumulator;
}

EditorUI::TextEditState& EditorUI::WidgetInteraction::GetTextEditState()
{
	return m_TextEdit;
}

const EditorUI::TextEditState& EditorUI::WidgetInteraction::GetTextEditState() const
{
	return m_TextEdit;
}
