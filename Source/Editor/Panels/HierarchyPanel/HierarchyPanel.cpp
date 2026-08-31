// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "HierarchyPanel.h"

#include <Engine/EditorUI/Widgets/Widgets.h>
#include <Engine/GameObject/GameObjectManager.h>

#include <Editor/Commands/ObjectCommands/ObjectCommands.h>
#include <Editor/Prefab/PrefabSystem/PrefabSystem.h>
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
//	2. オブジェクトの一覧（選択・名前変更・右クリックメニュー）
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

	GameObjectManager& objects = *_ctx.pObjects;

	// -------------------------------------------------------------------------------
	// フレームをまたいで覚えているポインタの生存確認
	//
	// 右クリックの対象と名前変更の対象は、どちらもメニューや入力を閉じるまで
	// 保持し続ける。その間に削除されている可能性があるため、毎フレーム確かめる
	// -------------------------------------------------------------------------------
	if (!GameObjectFactory::IsAlive(_ctx, m_pContextTarget))
	{
		m_pContextTarget = nullptr;
	}

	if (!GameObjectFactory::IsAlive(_ctx, m_pRenameTarget))
	{
		EndRename();
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

		DrawObjectRow(_ctx, *pObject, anyRowRightClicked);
	}

	// -------------------------------------------------------------------------------
	// F2 で選択中のオブジェクトの名前を変える
	//
	// 文字を入力している最中は反応させない
	// （検索欄に "F2" と打てなくなってしまうため）
	// -------------------------------------------------------------------------------
	if (ui.GetTextEditState().Widget == 0 &&
		ui.IsKeyPressed(EditorUI::Key::F2) &&
		ui.IsCurrentWindowHovered())
	{
		if (GameObject* pSelected = _ctx.pSelection->GetObject())
		{
			BeginRename(*pSelected);
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
// 1行ぶんの表示
// -------------------------------------------------------------------------------
void Editor::HierarchyPanel::DrawObjectRow(
	EditorContext&	_ctx,
	GameObject&		_object,
	bool&			_outRightClicked)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	Selection& selection = *_ctx.pSelection;

	// 同名のオブジェクトがあってもIdが衝突しないよう、ポインタでスコープを分ける
	ui.GetIdStack().PushPtr(&_object);

	if (m_pRenameTarget == &_object)
	{
		// 名前を変更中の行は、選択行ではなく入力欄として描く
		DrawRenameRow(_ctx, _object);
	}
	else
	{
		const EditorUI::ItemInteraction interaction = EditorUI::Selectable(
			ui, font, _object.GetName(), selection.IsObjectSelected(&_object));

		if (interaction.Clicked)
		{
			selection.SelectObject(&_object);
		}

		// 右クリックはその行を選択したうえでメニューを開く
		// 選ばずにメニューを出すと、どれに対する操作か分からなくなるため
		if (interaction.RightClicked)
		{
			selection.SelectObject(&_object);
			m_pContextTarget	= &_object;
			_outRightClicked	= true;
		}
	}

	ui.GetIdStack().Pop();
}

// -------------------------------------------------------------------------------
// 名前変更中の行
// -------------------------------------------------------------------------------
void Editor::HierarchyPanel::DrawRenameRow(EditorContext& _ctx, GameObject& _object)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	EditorUI::InputTextOptions options;
	options.CommitOnFocusLoss	= true;		// 他をクリックしたらその内容で確定する
	options.SelectAllOnFocus	= true;
	options.ActivateNow			= m_RenameActivate;

	// 合図は1フレームだけ。立てたままにすると編集状態から抜けられなくなる
	m_RenameActivate = false;

	const bool committed = EditorUI::InputText(ui, "##rename", font, &m_RenameBuffer, options);

	// -------------------------------------------------------------------------------
	// 終了の判定
	//
	//	確定した				… 名前を変更して終了
	//	Escapeで破棄された		… 何もせず終了
	//	どこも編集していない	… 別の場所をクリックした。終了
	// -------------------------------------------------------------------------------
	if (committed)
	{
		ObjectCommands::Rename(_ctx, &_object, m_RenameBuffer);
		EndRename();
		return;
	}

	if (ui.GetTextEditState().Widget == 0)
	{
		EndRename();
	}
}

// -------------------------------------------------------------------------------
// 名前の変更を始める
// -------------------------------------------------------------------------------
void Editor::HierarchyPanel::BeginRename(GameObject& _object)
{
	m_pRenameTarget		= &_object;
	m_RenameBuffer		= _object.GetName();
	m_RenameActivate	= true;		// 次の1フレームで入力欄へ移る
}

// -------------------------------------------------------------------------------
// 名前の変更をやめる
// -------------------------------------------------------------------------------
void Editor::HierarchyPanel::EndRename()
{
	m_pRenameTarget		= nullptr;
	m_RenameActivate	= false;
	m_RenameBuffer.clear();
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
// 実際の処理は ObjectCommands が持つ
// メニューバーの「作成」と同じ関数を呼ぶため、挙動が食い違わず、
// どちらから行ってもUndoで取り消せる
// -------------------------------------------------------------------------------
void Editor::HierarchyPanel::DrawContextMenu(EditorContext& _ctx, GameObject* _pTarget)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	// -------------------------------------------------------------------------------
	// 作成
	// -------------------------------------------------------------------------------
	if (EditorUI::MenuItem(ui, font, "空のオブジェクトを作成"))
	{
		ObjectCommands::CreateEmpty(_ctx, "New Object");
	}

	if (_pTarget == nullptr)
	{
		return;	// 空白部分ではここまで
	}

	EditorUI::MenuSeparator(ui);

	// -------------------------------------------------------------------------------
	// 選択中のオブジェクトに対する操作
	// -------------------------------------------------------------------------------
	if (EditorUI::MenuItem(ui, font, "名前を変更  (F2)"))
	{
		BeginRename(*_pTarget);
	}

	if (EditorUI::MenuItem(ui, font, "複製  (Ctrl+D)"))
	{
		ObjectCommands::Duplicate(_ctx, _pTarget);
	}

	if (EditorUI::MenuItem(ui, font, _pTarget->IsActive() ? "非アクティブにする" : "アクティブにする"))
	{
		ObjectCommands::SetActive(_ctx, _pTarget, !_pTarget->IsActive());
	}

	EditorUI::MenuSeparator(ui);

	// -------------------------------------------------------------------------------
	// プレファブとして保存
	//
	// 同じ構成を繰り返し使えるよう、今の状態をコンテンツフォルダへ書き出す
	// -------------------------------------------------------------------------------
	if (EditorUI::MenuItem(ui, font, "プレファブとして保存"))
	{
		std::filesystem::path savedPath;

		if (PrefabSystem::SaveToContent(_ctx, *_pTarget, savedPath))
		{
			_ctx.pSelection->SelectAsset(savedPath);
		}
	}

	EditorUI::MenuSeparator(ui);

	if (EditorUI::MenuItem(ui, font, "削除  (Delete)"))
	{
		ObjectCommands::Destroy(_ctx, _pTarget);

		// 消えたオブジェクトを指したままにしない
		m_pContextTarget = nullptr;

		if (m_pRenameTarget == _pTarget)
		{
			EndRename();
		}
	}
}
