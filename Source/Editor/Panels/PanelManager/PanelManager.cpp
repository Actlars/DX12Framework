// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "PanelManager.h"

// -------------------------------------------------------------------------------
// パネルの登録
// -------------------------------------------------------------------------------
Editor::IEditorPanel* Editor::PanelManager::Add(std::unique_ptr<IEditorPanel> _panel)
{
	if (_panel == nullptr)
	{ return nullptr; }

	IEditorPanel* pRaw = _panel.get();

	// 描画中はm_Panelsを走査しているため、その場で足すと反復子が壊れる
	if (m_Drawing)
	{
		m_PendingPanels.push_back(std::move(_panel));
	}
	else
	{
		m_Panels.push_back(std::move(_panel));
	}

	return pRaw;
}

// -------------------------------------------------------------------------------
// すべてのパネルを描く
//
// ウィンドウの開始と終了をここで一括して行うことで、
// 各パネルはBegin/Endの対応漏れを起こしようがなくなる
// -------------------------------------------------------------------------------
void Editor::PanelManager::DrawAll(EditorContext& _ctx)
{
	if (!_ctx.IsValid())
	{ return; }

	EditorUI::Context& ui = *_ctx.pUI;

	m_Drawing = true;

	for (const auto& panel : m_Panels)
	{
		if (panel == nullptr || !panel->IsOpen())
		{
			continue;
		}

		// まだEditorUI側に実体がないパネルにだけ初期配置を伝える
		// 2回目以降は、ユーザーが動かした位置とドッキング状態が優先される
		if (panel->GetWindowId() == 0)
		{
			ui.SetNextWindowPlacement(panel->GetInitialPosition(), panel->GetInitialSize());
		}

		// 閉じられるパネルだけbool*を渡す。渡すとタイトルバーに×が出る
		bool* pOpenFlag = panel->IsClosable() ? &panel->GetOpenFlag() : nullptr;

		const bool visible = ui.BeginWindow(panel->GetTitle(), pOpenFlag, panel->GetWindowFlags());

		// 非アクティブなタブでもWindowFrameは作られるため、ここでIdを控えられる
		if (const EditorUI::WindowFrame* pFrame = ui.GetCurrentWindow())
		{
			if (pFrame->pState != nullptr)
			{
				panel->SetWindowId(pFrame->pState->WindowId);
			}
		}

		if (visible)
		{
			panel->OnGUI(_ctx);
		}

		ui.EndWindow();
	}

	m_Drawing = false;

	// 描画中に増えたパネルを取り込む。次のフレームから表示される
	for (auto& pending : m_PendingPanels)
	{
		m_Panels.push_back(std::move(pending));
	}
	m_PendingPanels.clear();

	RemoveClosedPanels(_ctx);
}

// -------------------------------------------------------------------------------
// タイトルからパネルを探す
// -------------------------------------------------------------------------------
Editor::IEditorPanel* Editor::PanelManager::Find(std::string_view _title)
{
	const auto match = [_title](const std::unique_ptr<IEditorPanel>& _panel)
	{
		return _panel != nullptr && _panel->GetTitle() == _title;
	};

	auto it = std::find_if(m_Panels.begin(), m_Panels.end(), match);
	if (it != m_Panels.end())
	{
		return it->get();
	}

	// まだ取り込まれていない保留中のパネルも対象にする
	// 同じフレームに二重で開かれるのを防ぐため
	auto pendingIt = std::find_if(m_PendingPanels.begin(), m_PendingPanels.end(), match);
	return (pendingIt != m_PendingPanels.end()) ? pendingIt->get() : nullptr;
}

bool Editor::PanelManager::Open(std::string_view _title)
{
	IEditorPanel* pPanel = Find(_title);
	if (pPanel == nullptr)
	{ return false; }

	pPanel->SetOpen(true);
	return true;
}

// -------------------------------------------------------------------------------
// 閉じられた一時パネルの後片付け
//
// EditorUI側のウィンドウも破棄しないと、
// ドックのタブやZ順に「中身のないウィンドウ」が残り続ける
// -------------------------------------------------------------------------------
void Editor::PanelManager::RemoveClosedPanels(EditorContext& _ctx)
{
	for (const auto& panel : m_Panels)
	{
		if (panel == nullptr || panel->IsOpen())
		{
			continue;
		}

		if (panel->GetWindowId() != 0)
		{
			// 実際の削除は次フレームの先頭で行われる（安全なフレーム境界）
			_ctx.pUI->RequestDestroyWindow(panel->GetWindowId());
			panel->SetWindowId(0);
		}
	}

	// 常設パネルは閉じても状態を残したいので、破棄するのは一時パネルだけ
	m_Panels.erase(
		std::remove_if(m_Panels.begin(), m_Panels.end(),
			[](const std::unique_ptr<IEditorPanel>& _panel)
			{
				return _panel == nullptr || (!_panel->IsOpen() && _panel->IsTransient());
			}),
		m_Panels.end());
}
