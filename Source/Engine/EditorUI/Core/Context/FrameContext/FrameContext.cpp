#include "FrameContext.h"

// -------------------------------------------------------------------------------
// フレーム開始時の初期化処理
// -------------------------------------------------------------------------------
void EditorUI::FrameContext::NewFrame()
{
	m_WindowStack.clear();
	m_FrameByWindow.clear();
	m_Frames.clear();
	m_Overlay.Reset();
	m_Output.WindowDrawLists.clear();
}

// -------------------------------------------------------------------------------
// 一時Windowを生成し、描画リストとWindowStackに積む
// -------------------------------------------------------------------------------
EditorUI::WindowFrame& EditorUI::FrameContext::PushWindowFrame(EditorUI::WindowState& _state, EditorUI::WindowFlags& _flags)
{
	// 一時Windowを生成する
	auto frame		= std::make_unique<EditorUI::WindowFrame>();
	frame->pState	= &_state;
	frame->Flags	= _flags;
	frame->Draw.Reset();

	EditorUI::WindowFrame* raw = frame.get();
	m_Frames.push_back(std::move(frame));
	m_FrameByWindow[_state.WindowId] = raw;
	m_WindowStack.push_back(raw);
	return *raw;
}

// -------------------------------------------------------------------------------
// WindowStackの末尾の要素を削除する
// -------------------------------------------------------------------------------
void EditorUI::FrameContext::PopWindowFrame()
{
	if (m_WindowStack.empty()) 
	{ return; }
	m_WindowStack.pop_back();
}

// -------------------------------------------------------------------------------
// WindowStackを返す
// -------------------------------------------------------------------------------
EditorUI::WindowFrame* EditorUI::FrameContext::GetCurrentWindow()
{
	return m_WindowStack.empty() ? nullptr : m_WindowStack.back();
}

const EditorUI::WindowFrame* EditorUI::FrameContext::GetCurrentWindow() const
{
	return m_WindowStack.empty() ? nullptr : m_WindowStack.back();
}

// -------------------------------------------------------------------------------
// 指定したIdのWindowを見つけて返す
// -------------------------------------------------------------------------------
EditorUI::WindowFrame* EditorUI::FrameContext::FindWindowFrame(EditorUI::Id _windowId)
{
	auto it = m_FrameByWindow.find(_windowId);
	return it == m_FrameByWindow.end() ? nullptr : it->second;
}

const EditorUI::WindowFrame* EditorUI::FrameContext::FindWindowFrame(EditorUI::Id _windowId) const
{
	auto it = m_FrameByWindow.find(_windowId);
	return it == m_FrameByWindow.end() ? nullptr : it->second;
}

// -------------------------------------------------------------------------------
// 描画リストに一時Windowを積む
// -------------------------------------------------------------------------------
void EditorUI::FrameContext::AppendDrawList(EditorUI::Id _windowId)
{
	EditorUI::WindowFrame* frame = FindWindowFrame(_windowId);
	if (frame == nullptr || frame->SkippedEntirely) 
	{ return; }

	m_Output.WindowDrawLists.push_back(&frame->Draw);
}

// -------------------------------------------------------------------------------
// 最前面用の描画リストを返す
// -------------------------------------------------------------------------------
EditorUI::DrawList& EditorUI::FrameContext::GetOverlayDrawList()
{
	return m_Overlay;
}

// -------------------------------------------------------------------------------
// オーバーレイを出力の最後（＝最前面）に積む
// -------------------------------------------------------------------------------
void EditorUI::FrameContext::AppendOverlayDrawList()
{
	m_Output.WindowDrawLists.push_back(&m_Overlay);
}

const EditorUI::FrameOutput& EditorUI::FrameContext::GetOutput() const
{
	return m_Output;
}
