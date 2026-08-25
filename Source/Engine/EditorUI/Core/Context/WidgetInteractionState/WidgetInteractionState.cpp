#include "WidgetInteractionState.h"

// -------------------------------------------------------------------------------
// 新しいフレーム開始時の状態更新
// -------------------------------------------------------------------------------
void EditorUI::WidgetInteractionState::NewFrame()
{
	// 前フレームでActiveだったWidgetが今フレームで生存申告されなかった場合
	// Widget自体が消えたものとしてActive状態を解除する
	if (m_ActiveId != 0 && !m_ActiveIdIsAlive)
	{
		m_ActiveId = 0;
		m_ActiveIdDragAccumulator = 0.0f;
	}

	// テキスト編集中のWidgetが前フレーム以降呼ばれなくなった場合
	// 編集結果を確定する対象が存在しないため編集状態を破棄する
	if (m_TextEdit.Widget != 0 && !m_TextEdit.Alive)
	{
		m_TextEdit.Reset();
	}

	// 今フレーム用に生存フラグをリセット
	m_ActiveIdIsAlive	= false;
	m_TextEdit.Alive	= false;
	m_HoveredId			= 0;
}

// -------------------------------------------------------------------------------
// 現在ActiveになっているWidgetIdを取得
// -------------------------------------------------------------------------------
EditorUI::Id EditorUI::WidgetInteractionState::GetActiveId() const
{
	return m_ActiveId;
}

// -------------------------------------------------------------------------------
// 指定Widgetが現在Active状態か判定
// -------------------------------------------------------------------------------
bool EditorUI::WidgetInteractionState::IsActive(EditorUI::Id _id) const
{
	return m_ActiveId != 0 && m_ActiveId == _id;
}

// -------------------------------------------------------------------------------
// いずれかのWidgetがActive状態か判定
// -------------------------------------------------------------------------------
bool EditorUI::WidgetInteractionState::IsAnyActive() const
{
	return m_ActiveId != 0;
}

// -------------------------------------------------------------------------------
// 指定WidgetをActive状態に設定
// -------------------------------------------------------------------------------
void EditorUI::WidgetInteractionState::SetActive(EditorUI::Id _id, const DirectX::XMFLOAT2& _mousePos)
{
	// すでに同じWidgetがActiveなら状態を作り直さず生存フラグだけ更新
	if (m_ActiveId == _id)
	{
		m_ActiveIdIsAlive = true;
		return;
	}

	m_ActiveId					= _id;			// 新しいWidgetへActive状態を切り替える
	m_ActiveIdIsAlive			= true;			// 今フレームでもWidgetが存在していることを記録
	m_ActiveIdClickPos			= _mousePos;	// WidgetをActiveにした瞬間のマウス座標を保存
	m_ActiveIdDragAccumulator	= 0.0f;			// 新しい操作を開始するためのドラッグ累積値をリセット
}

// -------------------------------------------------------------------------------
// 指定WidgetのActive状態を解除
// -------------------------------------------------------------------------------
void EditorUI::WidgetInteractionState::ClearActive(EditorUI::Id _id)
{
	// 別Widgetがすでに所有権をとっている場合は横取りして解除しない
	if (m_ActiveId != _id) 
	{ return; }

	m_ActiveId					= 0;
	m_ActiveIdDragAccumulator	= 0.0f;
}

// -------------------------------------------------------------------------------
// 指定WidgetのActive状態が今フレームでも有効であることを通知
// -------------------------------------------------------------------------------
void EditorUI::WidgetInteractionState::KeepActive(EditorUI::Id _id)
{
	if (m_ActiveId == _id)
	{
		m_ActiveIdIsAlive = true;
	}
}

// -------------------------------------------------------------------------------
// 現在HoverされているWidgetIdを取得
// -------------------------------------------------------------------------------
EditorUI::Id EditorUI::WidgetInteractionState::GetHoveredId() const
{
	return m_HoveredId;
}

// -------------------------------------------------------------------------------
// 指定Widgetが現在Hover状態か判定
// -------------------------------------------------------------------------------
bool EditorUI::WidgetInteractionState::IsHovered(EditorUI::Id _id) const
{
	return m_HoveredId != 0 && m_HoveredId == _id;
}

// -------------------------------------------------------------------------------
// 現在HoverされているWidgetを設定
// -------------------------------------------------------------------------------
void EditorUI::WidgetInteractionState::SetHovered(EditorUI::Id _id)
{
	m_HoveredId = _id;
}

// -------------------------------------------------------------------------------
// Active状態になった瞬間のマウス座標を取得
// -------------------------------------------------------------------------------
const DirectX::XMFLOAT2& EditorUI::WidgetInteractionState::GetActiveClickPos() const
{
	return m_ActiveIdClickPos;
}

// -------------------------------------------------------------------------------
// ActiveWidgetのドラッグ累積値を取得
// -------------------------------------------------------------------------------
double& EditorUI::WidgetInteractionState::GetDragAccumulator()
{
	return m_ActiveIdDragAccumulator;
}

// -------------------------------------------------------------------------------
// テキスト編集状態を取得
// -------------------------------------------------------------------------------
EditorUI::TextEditState& EditorUI::WidgetInteractionState::GetTextEditState()
{
	return m_TextEdit;
}

// -------------------------------------------------------------------------------
// テキスト編集状態を読み取り専用で取得
// -------------------------------------------------------------------------------
const EditorUI::TextEditState& EditorUI::WidgetInteractionState::GetTextEditState() const
{
	return m_TextEdit;
}
