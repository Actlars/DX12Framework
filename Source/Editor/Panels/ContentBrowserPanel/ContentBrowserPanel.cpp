// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "ContentBrowserPanel.h"

#include <Engine/EditorUI/Widgets/Widgets.h>
#include <Engine/EditorUI/Widgets/Layout/Layout.h>
#include <Engine/EditorUI/Text/Font/Font.h>
#include <Editor/Panels/PanelManager/PanelManager.h>
#include <Editor/Panels/EffectEditorPanel/EffectEditorPanel.h>
#include <Editor/Effect/EffectAsset.h>

namespace
{
	// メニュー名。Idの元になるため、パネル内で一意にする
	constexpr std::string_view kItemContextMenu	= "ContentItemMenu";
	constexpr std::string_view kEmptyContextMenu = "ContentEmptyMenu";

	// -------------------------------------------------------------------------------
	// 種類ごとのアイコン色
	//
	// アイコン画像を持たないため、色つきの小さな四角で種類を見分けられるようにする
	// 一覧をざっと眺めたときに、探しているものへ目が行きやすくなる
	// -------------------------------------------------------------------------------
	EditorUI::Color32 GetTypeColor(Editor::AssetType _type)
	{
		using Editor::AssetType;

		switch (_type)
		{
		case AssetType::Folder:		return EditorUI::MakeColor(226, 186,  96);
		case AssetType::Effect:		return EditorUI::MakeColor(232, 124,  86);
		case AssetType::Scene:		return EditorUI::MakeColor(108, 176, 232);
		case AssetType::Model:		return EditorUI::MakeColor(126, 200, 126);
		case AssetType::Texture:	return EditorUI::MakeColor(186, 130, 216);
		case AssetType::Shader:		return EditorUI::MakeColor( 96, 196, 196);
		case AssetType::Text:		return EditorUI::MakeColor(160, 164, 172);
		default:					return EditorUI::MakeColor(120, 124, 132);
		}
	}
}

// -------------------------------------------------------------------------------
// コンストラクタ
//
// 2枚目以降は末尾に番号を付ける
// タイトルが重複するとEditorUI側で同じウィンドウとみなされ、
// 2枚目が1枚目に重なって表示されてしまう
// -------------------------------------------------------------------------------
Editor::ContentBrowserPanel::ContentBrowserPanel(int _index)
	: m_Title(_index <= 1 ? std::string("Content Browser") : "Content Browser " + std::to_string(_index))
{
	SetInitialPlacement({ 20.0f, 500.0f }, { 560.0f, 220.0f });
}

// -------------------------------------------------------------------------------
// コンテンツブラウザの中身
// -------------------------------------------------------------------------------
void Editor::ContentBrowserPanel::OnGUI(EditorContext& _ctx)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	if (_ctx.pAssets == nullptr)
	{
		EditorUI::TextMuted(ui, font, "コンテンツフォルダが初期化されていません");
		return;
	}

	// -------------------------------------------------------------------------------
	// 外部での変更を先に取り込む
	//
	// 一覧を描き始めたあとに読み直すと、走査中のvectorが入れ替わってしまう
	// 必ずこのフレームの描画より前に済ませる
	// -------------------------------------------------------------------------------
	_ctx.pAssets->Update();

	DrawBreadcrumb(_ctx);
	EditorUI::Separator(ui);
	DrawEntries(_ctx);
}

// -------------------------------------------------------------------------------
// パンくずリスト
//
// 「上へ」と、ルートから現在位置までのボタンを1行に並べる
// 深い階層に入っても、1クリックで任意の階層へ戻れるようにするため
// -------------------------------------------------------------------------------
void Editor::ContentBrowserPanel::DrawBreadcrumb(EditorContext& _ctx)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;
	AssetDatabase&		assets	= *_ctx.pAssets;

	if (EditorUI::Button(ui, "上へ", font, { 48.0f, 22.0f }))
	{
		assets.GoUp();
	}

	const auto& breadcrumb = assets.GetBreadcrumb();

	for (std::size_t i = 0; i < breadcrumb.size(); ++i)
	{
		EditorUI::SameLine(ui);

		// ルートだけは名前が空になり得るので、固定の表示名にする
		const std::string label = (i == 0)
			? std::string("Content")
			: breadcrumb[i].filename().string();

		// 同名フォルダがあってもIdが衝突しないよう、深さでスコープを分ける
		ui.GetIdStack().PushInt(static_cast<int>(i));

		if (EditorUI::Button(ui, label, font, { 0.0f, 22.0f }))
		{
			assets.SetCurrentDirectory(breadcrumb[i]);
		}

		ui.GetIdStack().Pop();
	}
}

// -------------------------------------------------------------------------------
// 項目一覧
// -------------------------------------------------------------------------------
void Editor::ContentBrowserPanel::DrawEntries(EditorContext& _ctx)
{
	EditorUI::Context&	ui			= *_ctx.pUI;
	EditorUI::Font&		font		= *_ctx.pFont;
	AssetDatabase&		assets		= *_ctx.pAssets;
	Selection&			selection	= *_ctx.pSelection;

	const EditorUI::Style& style = ui.GetStyle();

	const auto& entries = assets.GetEntries();

	if (entries.empty())
	{
		EditorUI::TextMuted(ui, font, "このフォルダは空です（右クリックで作成）");
	}

	bool anyRowRightClicked = false;

	for (const AssetEntry& entry : entries)
	{
		// パス文字列でスコープを分け、同名でも別Idになるようにする
		ui.GetIdStack().PushString(entry.Name);

		// -------------------------------------------------------------------------------
		// 行そのものはSelectableに任せ、その上へ種類アイコンを重ねて描く
		// 行の描画とアイコンの描画を分けることで、
		// Selectable側の見た目（ホバー・選択）をそのまま使い回せる
		// -------------------------------------------------------------------------------
		const EditorUI::ItemInteraction interaction = EditorUI::Selectable(
			ui, font, "    " + entry.Name, selection.IsAssetSelected(entry.Path));

		ui.GetIdStack().Pop();

		if (EditorUI::WindowFrame* pFrame = ui.GetCurrentWindow())
		{
			const EditorUI::Rect2D& row = pFrame->LastItemRect;

			// 行の左端に、種類を表す小さな四角を置く
			const float iconSize = font.GetLineHeight() * 0.55f;
			const EditorUI::Rect2D iconRect = EditorUI::MakeRect(
				{
					row.Min.x + style.FramePaddingX,
					row.Min.y + (row.Height() - iconSize) * 0.5f
				},
				{ iconSize, iconSize });

			pFrame->Draw.AddRectFilled(iconRect, GetTypeColor(entry.Type));
		}

		if (interaction.Clicked)
		{
			selection.SelectAsset(entry.Path);
		}

		if (interaction.DoubleClicked)
		{
			OpenEntry(_ctx, entry);

			// フォルダを開いた場合、この時点でentriesの中身が入れ替わっている
			// 無効になった参照をこれ以上たどらないよう、ここでループを抜ける
			return;
		}

		if (interaction.RightClicked)
		{
			selection.SelectAsset(entry.Path);

			m_ContextTarget		= entry;
			m_HasContextTarget	= true;
			anyRowRightClicked	= true;
		}
	}

	// -------------------------------------------------------------------------------
	// 右クリックメニュー
	// -------------------------------------------------------------------------------
	if (EditorUI::BeginPopupContextItem(ui, kItemContextMenu, anyRowRightClicked))
	{
		DrawContextMenu(_ctx, m_HasContextTarget ? &m_ContextTarget : nullptr);
		EditorUI::EndPopup(ui);
	}

	if (EditorUI::BeginPopupContextWindow(ui, kEmptyContextMenu))
	{
		DrawContextMenu(_ctx, nullptr);
		EditorUI::EndPopup(ui);
	}
}

// -------------------------------------------------------------------------------
// エントリを開く（ダブルクリック時の動作）
// -------------------------------------------------------------------------------
void Editor::ContentBrowserPanel::OpenEntry(EditorContext& _ctx, const AssetEntry& _entry)
{
	if (_entry.IsDirectory)
	{
		_ctx.pAssets->SetCurrentDirectory(_entry.Path);
		return;
	}

	// エフェクトは専用ウィンドウで開く
	// 他の種類も、対応するエディタができ次第ここへ足していく
	if (_entry.Type == AssetType::Effect)
	{
		EffectEditorPanel::OpenForAsset(_ctx, _entry.Path);
	}
}

// -------------------------------------------------------------------------------
// 右クリックメニューの中身
// -------------------------------------------------------------------------------
void Editor::ContentBrowserPanel::DrawContextMenu(EditorContext& _ctx, const AssetEntry* _pTarget)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;
	AssetDatabase&		assets	= *_ctx.pAssets;

	// -------------------------------------------------------------------------------
	// 対象がある場合の操作
	// -------------------------------------------------------------------------------
	if (_pTarget != nullptr)
	{
		if (EditorUI::MenuItem(ui, font, _pTarget->IsDirectory ? "フォルダを開く" : "開く"))
		{
			// メニューを閉じたあとに開く必要はないため、その場で実行してよい
			OpenEntry(_ctx, *_pTarget);
		}

		if (EditorUI::MenuItem(ui, font, "削除"))
		{
			assets.Delete(_pTarget->Path);

			// 消したものを選んだままにしない
			if (_ctx.pSelection->IsAssetSelected(_pTarget->Path))
			{
				_ctx.pSelection->Clear();
			}

			m_HasContextTarget = false;
		}

		EditorUI::MenuSeparator(ui);
	}

	// -------------------------------------------------------------------------------
	// 新規作成
	// 対象の有無にかかわらず、常に現在のフォルダへ作る
	// -------------------------------------------------------------------------------
	if (EditorUI::MenuItem(ui, font, "新しいフォルダ"))
	{
		CreateNewFolder(_ctx);
	}

	if (EditorUI::MenuItem(ui, font, "新しいテキストファイル"))
	{
		CreateNewTextFile(_ctx);
	}

	if (EditorUI::MenuItem(ui, font, "新しいエフェクト"))
	{
		CreateNewEffect(_ctx, false);
	}

	if (EditorUI::MenuItem(ui, font, "新しいエフェクト（エディタで開く）"))
	{
		CreateNewEffect(_ctx, true);
	}

	EditorUI::MenuSeparator(ui);

	if (EditorUI::MenuItem(ui, font, "再読み込み"))
	{
		assets.Refresh();
	}
}

// -------------------------------------------------------------------------------
// 新規作成
// -------------------------------------------------------------------------------
void Editor::ContentBrowserPanel::CreateNewFolder(EditorContext& _ctx)
{
	std::filesystem::path created;

	if (_ctx.pAssets->CreateFolder("NewFolder", created))
	{
		_ctx.pSelection->SelectAsset(created);
	}
}

void Editor::ContentBrowserPanel::CreateNewTextFile(EditorContext& _ctx)
{
	std::filesystem::path created;

	if (_ctx.pAssets->CreateFile("NewFile", ".txt", "", created))
	{
		_ctx.pSelection->SelectAsset(created);
	}
}

// -------------------------------------------------------------------------------
// 新規エフェクト
//
// 空ファイルではなく、既定値を書き出した状態で作る
// そのまま開いてもプレビューが動き、何も見えないという状態にならないようにするため
// -------------------------------------------------------------------------------
void Editor::ContentBrowserPanel::CreateNewEffect(EditorContext& _ctx, bool _openEditor)
{
	EffectAsset defaults;
	defaults.Name = "NewEffect";

	const std::string contents = SerializeEffect(defaults);

	std::filesystem::path created;
	if (!_ctx.pAssets->CreateFile("NewEffect", ".effect", contents, created))
	{
		return;
	}

	_ctx.pSelection->SelectAsset(created);

	if (_openEditor)
	{
		EffectEditorPanel::OpenForAsset(_ctx, created);
	}
}
