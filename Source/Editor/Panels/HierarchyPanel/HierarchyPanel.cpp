// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "HierarchyPanel.h"

#include <Engine/EditorUI/Widgets/Widgets.h>
#include <Engine/GameObject/GameObjectManager.h>
#include <Engine/GameObject/Components/TransformComponent/TransformComponent.h>
#include <Editor/Scene/GameObjectFactory/GameObjectFactory.h>

namespace
{
	// 右クリックメニューの名前。Idの元になるため、パネル内で一意にする
	constexpr std::string_view kObjectContextMenu	= "HierarchyObjectMenu";
	constexpr std::string_view kEmptyContextMenu	= "HierarchyEmptyMenu";

	// 大文字小文字を無視した部分一致
	bool ContainsIgnoreCase(std::string_view _text, std::string_view _pattern)
	{
		if (_pattern.empty())
		{ return true; }

		const auto it = std::search(
			_text.begin(), _text.end(),
			_pattern.begin(), _pattern.end(),
			[](char _a, char _b)
			{
				return std::tolower(static_cast<unsigned char>(_a)) ==
					   std::tolower(static_cast<unsigned char>(_b));
			});

		return it != _text.end();
	}
}

// -------------------------------------------------------------------------------
// コンストラクタ
//
// 2枚目以降は末尾に番号を付ける
// タイトルが重複するとEditorUI側で同じウィンドウとみなされ、
// 2枚目が1枚目に重なって表示されてしまう
// -------------------------------------------------------------------------------
Editor::HierarchyPanel::HierarchyPanel(int _index)
	: m_Title(_index <= 1 ? std::string("Hierarchy") : "Hierarchy " + std::to_string(_index))
{
	SetInitialPlacement({ 20.0f, 60.0f }, { 260.0f, 420.0f });
}

// -------------------------------------------------------------------------------
// ヒエラルキーの表示
//
// 構成
//	1. 検索欄
//	2. オブジェクトの一覧（選択・右クリックメニュー）
//	3. 何もない場所での右クリックメニュー
// -------------------------------------------------------------------------------
void Editor::HierarchyPanel::OnGUI(EditorContext& _ctx)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	if (_ctx.pObjects == nullptr)
	{
		EditorUI::TextMuted(ui, font, "シーンが読み込まれていません");
		return;
	}

	GameObjectManager&	objects		= *_ctx.pObjects;
	Selection&			selection	= *_ctx.pSelection;

	// -------------------------------------------------------------------------------
	// 右クリック対象の生存確認
	//
	// 生ポインタをフレームをまたいで持つため、
	// 削除されたオブジェクトを指したままメニューを開かないようにする
	// -------------------------------------------------------------------------------
	if (m_pContextTarget != nullptr)
	{
		const auto& allObjects = objects.GetObjects();

		const bool alive = std::any_of(allObjects.begin(), allObjects.end(),
			[this](const std::unique_ptr<GameObject>& _candidate)
			{
				return _candidate.get() == m_pContextTarget;
			});

		if (!alive)
		{
			m_pContextTarget = nullptr;
		}
	}

	// -------------------------------------------------------------------------------
	// 検索欄
	// -------------------------------------------------------------------------------
	EditorUI::InputTextOptions filterOptions;
	filterOptions.CommitOnFocusLoss = true;

	EditorUI::Property(ui, font, "Search", &m_Filter, filterOptions, { 56.0f, 0.0f, 4.0f });

	EditorUI::SeparatorText(ui, font,
		"Objects (" + std::to_string(objects.ObjectCount()) + ")");

	// -------------------------------------------------------------------------------
	// オブジェクトの一覧
	//
	// 現状のGameObjectは親子関係を持たないため、まずは平坦な一覧として並べる
	// 階層が入ったときにTreeNodeへ差し替えられるよう、行の描画は1か所にまとめてある
	// -------------------------------------------------------------------------------
	bool anyRowRightClicked = false;

	for (const auto& objectPtr : objects.GetObjects())
	{
		GameObject* pObject = objectPtr.get();
		if (pObject == nullptr || !MatchesFilter(pObject->GetName()))
		{
			continue;
		}

		// 同名のオブジェクトがあってもIdが衝突しないよう、ポインタでスコープを分ける
		ui.GetIdStack().PushPtr(pObject);

		const EditorUI::ItemInteraction interaction = EditorUI::Selectable(
			ui, font, pObject->GetName(), selection.IsObjectSelected(pObject));

		ui.GetIdStack().Pop();

		if (interaction.Clicked)
		{
			selection.SelectObject(pObject);
		}

		// 右クリックはその行を選択したうえでメニューを開く
		// 選ばずにメニューを出すと、どれに対する操作か分からなくなるため
		if (interaction.RightClicked)
		{
			selection.SelectObject(pObject);
			m_pContextTarget	= pObject;
			anyRowRightClicked	= true;
		}
	}

	// -------------------------------------------------------------------------------
	// 右クリックメニュー
	//
	//	行の上   … 対象つきのメニュー
	//	空白の上 … 作成だけのメニュー
	// -------------------------------------------------------------------------------
	if (EditorUI::BeginPopupContextItem(ui, kObjectContextMenu, anyRowRightClicked))
	{
		DrawContextMenu(_ctx, m_pContextTarget);
		EditorUI::EndPopup(ui);
	}

	if (EditorUI::BeginPopupContextWindow(ui, kEmptyContextMenu))
	{
		DrawContextMenu(_ctx, nullptr);
		EditorUI::EndPopup(ui);
	}
}

// -------------------------------------------------------------------------------
// 検索欄による絞り込み
// -------------------------------------------------------------------------------
bool Editor::HierarchyPanel::MatchesFilter(std::string_view _name) const
{
	return ContainsIgnoreCase(_name, m_Filter);
}

// -------------------------------------------------------------------------------
// 右クリックメニューの中身
//
// _pTargetがnullptrなら「空白部分のメニュー」として、作成の項目だけを出す
// -------------------------------------------------------------------------------
void Editor::HierarchyPanel::DrawContextMenu(EditorContext& _ctx, GameObject* _pTarget)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	// -------------------------------------------------------------------------------
	// 作成
	//
	// 実際の処理は GameObjectFactory が持つ
	// メニューバーの「作成」と同じ関数を呼ぶため、挙動が食い違わない
	// -------------------------------------------------------------------------------
	if (EditorUI::MenuItem(ui, font, "空のオブジェクトを作成"))
	{
		GameObjectFactory::CreateEmpty(_ctx, "New Object");
	}

	if (_pTarget == nullptr)
	{
		return;	// 空白部分ではここまで
	}

	EditorUI::MenuSeparator(ui);

	// -------------------------------------------------------------------------------
	// 選択中のオブジェクトに対する操作
	// -------------------------------------------------------------------------------
	if (EditorUI::MenuItem(ui, font, "複製"))
	{
		GameObjectFactory::Duplicate(_ctx, _pTarget);
	}

	if (EditorUI::MenuItem(ui, font, _pTarget->IsActive() ? "非アクティブにする" : "アクティブにする"))
	{
		_pTarget->SetActive(!_pTarget->IsActive());
	}

	EditorUI::MenuSeparator(ui);

	if (EditorUI::MenuItem(ui, font, "削除"))
	{
		GameObjectFactory::Destroy(_ctx, _pTarget);

		// 消えたオブジェクトを指したままにしない
		m_pContextTarget = nullptr;
	}
}
