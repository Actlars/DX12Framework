// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "CommandHistory.h"
#include <Editor/EditorContext.h>

// -------------------------------------------------------------------------------
// 操作を実行し、履歴へ積む
// -------------------------------------------------------------------------------
bool Editor::CommandHistory::Execute(
	EditorContext&					_ctx,
	std::unique_ptr<IEditorCommand>	_pCommand)
{
	if (_pCommand == nullptr)
	{
		return false;
	}

	// 実行できなかった操作は積まない
	// 「戻しても何も起きない項目」で履歴が埋まるのを防ぐ
	if (!_pCommand->Execute(_ctx))
	{
		return false;
	}

	// -------------------------------------------------------------------------------
	// ここから先の未来は分岐したため、やり直せる山は捨てる
	// -------------------------------------------------------------------------------
	m_RedoStack.clear();

	// 実行済みコマンドをUndo履歴に登録
	m_UndoStack.emplace_back(std::move(_pCommand));

	// 上限を超えたぶんは、いちばん古い操作から捨てる
	if (m_UndoStack.size() > kMaxHistory)
	{
		// 101個だった場合
		// 101 - 100 = 1, 
		// m_UndoStackの一番先頭から1個消し、残り残数が100になる
		const size_t excess = m_UndoStack.size() - kMaxHistory;
		// begin + excessは、1の場合、beginのみ削除、 + 1しているが、2件は消さない
		m_UndoStack.erase(m_UndoStack.begin(), m_UndoStack.begin() + excess);
	}

	return true;
}

// -------------------------------------------------------------------------------
// 元に戻す
// -------------------------------------------------------------------------------
bool Editor::CommandHistory::Undo(EditorContext& _ctx)
{
	if (m_UndoStack.empty())
	{
		return false;
	}

	// -------------------------------------------------------------------------------
	// 最新CommandをUndoStackから取り出す
	// -------------------------------------------------------------------------------
	
	// backはCommand末尾(最後に実行したCommand)を取得する
	// std::moveによってCommandの所有権を一時変数へ移す
	std::unique_ptr<IEditorCommand> pCommand = std::move(m_UndoStack.back());
	// Stack末尾はmove後の空unique_ptrが残るため、その要素自体を削除する
	m_UndoStack.pop_back();

	// Commandが保存している以前の状態へ戻す
	pCommand->Undo(_ctx);

	// Undo後に再び戻せるようにRedoにコマンドを移す
	m_RedoStack.emplace_back(std::move(pCommand));

	return true;
}

// -------------------------------------------------------------------------------
// やり直す
// -------------------------------------------------------------------------------
bool Editor::CommandHistory::Redo(EditorContext& _ctx)
{
	if (m_RedoStack.empty())
	{
		return false;
	}

	// -------------------------------------------------------------------------------
	// 次にRedoするコマンドを取り出す
	// -------------------------------------------------------------------------------
	std::unique_ptr<IEditorCommand> pCommand = std::move(m_RedoStack.back());
	m_RedoStack.pop_back();

	// -------------------------------------------------------------------------------
	// やり直せなかった場合は、その操作をそのまま捨てる
	//
	// 戻せる山へ積むと「戻しても何も起きない項目」が残ってしまう
	// -------------------------------------------------------------------------------
	if (!pCommand->Execute(_ctx))
	{
		return false;
	}

	// Redo後に再び戻せるようにUndoにコマンドを移す
	m_UndoStack.emplace_back(std::move(pCommand));

	return true;
}

// -------------------------------------------------------------------------------
// 次に戻る操作の名前
// Menu表示で
// 元の戻す : オブジェクト移動
// のような文字列を作成するために使用する
// -------------------------------------------------------------------------------
std::string_view Editor::CommandHistory::GetUndoLabel() const
{
	if (m_UndoStack.empty())
	{
		return {};
	}

	return m_UndoStack.back()->GetLabel();
}

// -------------------------------------------------------------------------------
// 次に進む操作の名前
// -------------------------------------------------------------------------------
std::string_view Editor::CommandHistory::GetRedoLabel() const
{
	if (m_RedoStack.empty())
	{
		return {};
	}

	return m_RedoStack.back()->GetLabel();
}

// -------------------------------------------------------------------------------
// 履歴をすべて捨てる
// -------------------------------------------------------------------------------
void Editor::CommandHistory::Clear()
{
	m_RedoStack.clear();
	m_UndoStack.clear();
}
