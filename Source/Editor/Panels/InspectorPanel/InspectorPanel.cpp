// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "InspectorPanel.h"

#include <Engine/EditorUI/Widgets/Widgets.h>
#include <Engine/GameObject/GameObject.h>

#include <Editor/Assets/AssetDatabase.h>
#include <Editor/Commands/ComponentCommands/ComponentCommands.h>
#include <Editor/Commands/ObjectCommands/ObjectCommands.h>
#include <Editor/Components/ComponentRegistry/ComponentRegistry.h>
#include <Editor/Panels/PanelManager/PanelManager.h>
#include <Editor/Panels/EffectEditorPanel/EffectEditorPanel.h>
#include <Editor/Prefab/PrefabSystem/PrefabSystem.h>

namespace
{
	// インスペクタ全体で共有するレイアウト
	// 同じ値を使うことで、どの行もラベルの右端がそろう
	constexpr EditorUI::PropertyLayout kInspectorLayout{ 88.0f, 0.0f, 4.0f };

	// 「コンポーネントを追加」で開くメニュー。Idの元になるためパネル内で一意にする
	constexpr std::string_view kAddComponentMenu = "InspectorAddComponentMenu";
}

// -------------------------------------------------------------------------------
// コンストラクタ
//
// 2枚目以降は末尾に番号を付ける
// タイトルが重複するとEditorUI側で同じウィンドウとみなされ、
// 2枚目が1枚目に重なって表示されてしまう
// -------------------------------------------------------------------------------
Editor::InspectorPanel::InspectorPanel(int _index)
	: m_Title(_index <= 1 ? std::string("Inspector") : "Inspector " + std::to_string(_index))
{
	SetInitialPlacement({ 980.0f, 60.0f }, { 300.0f, 420.0f });
}

// -------------------------------------------------------------------------------
// 選択の種類に応じて表示を切り替える
// -------------------------------------------------------------------------------
void Editor::InspectorPanel::OnGUI(EditorContext& _ctx)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	Selection& selection = *_ctx.pSelection;

	switch (selection.GetType())
	{
	case SelectionType::SceneObject:
	{
		GameObject* pObject = selection.GetObject();
		if (pObject != nullptr)
		{
			DrawObjectInspector(_ctx, *pObject);
		}
		break;
	}

	case SelectionType::Asset:
		DrawAssetInspector(_ctx);
		break;

	case SelectionType::None:
	default:
		EditorUI::TextMuted(ui, font, "何も選択されていません");
		EditorUI::TextMuted(ui, font, "ヒエラルキーかコンテンツブラウザで選択してください");
		break;
	}
}

// -------------------------------------------------------------------------------
// GameObjectの中身
//
// 上から
//	1. 基本情報（名前・アクティブ・ID）
//	2. 持っているコンポーネント
//	3. コンポーネントの追加と、プレファブとしての保存
// -------------------------------------------------------------------------------
void Editor::InspectorPanel::DrawObjectInspector(EditorContext& _ctx, GameObject& _object)
{
	EditorUI::Context& ui = *_ctx.pUI;

	DrawObjectHeader(_ctx, _object);

	EditorUI::Separator(ui);

	DrawComponentSections(_ctx, _object);

	EditorUI::Separator(ui);

	DrawAddComponentMenu(_ctx, _object);

	// -------------------------------------------------------------------------------
	// プレファブとして保存
	//
	// 今の構成を設計図としてコンテンツフォルダへ書き出す
	// 以後は同じ構成をいくつでも置けるようになる
	// -------------------------------------------------------------------------------
	if (EditorUI::Button(ui, "プレファブとして保存", *_ctx.pFont, { 200.0f, 24.0f }))
	{
		std::filesystem::path savedPath;

		if (PrefabSystem::SaveToContent(_ctx, _object, savedPath))
		{
			// 保存先が分かるよう、作られたファイルをそのまま選択する
			_ctx.pSelection->SelectAsset(savedPath);
		}
	}
}

// -------------------------------------------------------------------------------
// 基本情報
// -------------------------------------------------------------------------------
void Editor::InspectorPanel::DrawObjectHeader(EditorContext& _ctx, GameObject& _object)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	// -------------------------------------------------------------------------------
	// 名前
	//
	// 選択が切り替わったときだけ編集用のバッファへ写す
	// 毎フレーム写すと、入力中の文字が確定前に上書きされてしまう
	// -------------------------------------------------------------------------------
	if (m_NameBufferOwner != _object.GetID())
	{
		m_NameBufferOwner	= _object.GetID();
		m_NameBuffer		= _object.GetName();
	}

	if (EditorUI::Property(ui, font, "Name", &m_NameBuffer, {}, kInspectorLayout))
	{
		// -------------------------------------------------------------------------------
		// 確定した時点で履歴へ積む
		//
		// 重複する名前は連番が足されるため、実際に付いた名前を写し直す
		// そうしないと、入力欄と表示名が食い違ったままになる
		// -------------------------------------------------------------------------------
		ObjectCommands::Rename(_ctx, &_object, m_NameBuffer);

		m_NameBuffer = _object.GetName();
	}

	bool active = _object.IsActive();
	if (EditorUI::Property(ui, font, "Active", &active, kInspectorLayout))
	{
		ObjectCommands::SetActive(_ctx, &_object, active);
	}

	EditorUI::TextMuted(ui, font, "ID : " + std::to_string(_object.GetID()));
}

// -------------------------------------------------------------------------------
// 持っているコンポーネントを並べる
//
// 並び順は ComponentRegistry の登録順
// オブジェクトごとに順番が変わらないため、目で追いやすくなる
// -------------------------------------------------------------------------------
void Editor::InspectorPanel::DrawComponentSections(EditorContext& _ctx, GameObject& _object)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	bool anyComponent = false;

	for (const ComponentTypeInfo& info : ComponentRegistry::GetAll())
	{
		if (info.Has == nullptr || !info.Has(_object))
		{
			continue;	// 持っていない型は並べない
		}

		anyComponent = true;

		DrawComponentSection(_ctx, _object, info);
	}

	if (!anyComponent)
	{
		EditorUI::TextMuted(ui, font, "コンポーネントがありません");
	}
}

// -------------------------------------------------------------------------------
// コンポーネント1つぶんの枠
// -------------------------------------------------------------------------------
void Editor::InspectorPanel::DrawComponentSection(
	EditorContext&				_ctx,
	GameObject&					_object,
	const ComponentTypeInfo&	_typeInfo)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	// -------------------------------------------------------------------------------
	// 型ごとにIdの範囲を分ける
	//
	// MeshとMeshletはどちらも "Visible" という行を持つ
	// 同じ名前のままだとEditorUIが同じウィジェットとみなしてしまうため、
	// 型ごとの目印でスコープを切っておく
	// -------------------------------------------------------------------------------
	ui.GetIdStack().PushPtr(&_typeInfo);

	if (EditorUI::CollapsingHeader(ui, font, _typeInfo.DisplayName))
	{
		// 中身は型ごとの担当へ任せる
		if (_typeInfo.DrawInspector != nullptr)
		{
			_typeInfo.DrawInspector(_ctx, _object);
		}

		// -------------------------------------------------------------------------------
		// 取り外し
		//
		// Transformのように外せない型ではボタン自体を出さない
		// 押せないボタンを並べるより、無いほうが迷わない
		// -------------------------------------------------------------------------------
		if (_typeInfo.IsRemovable)
		{
			if (EditorUI::Button(ui, "コンポーネントを削除", font, { 180.0f, 22.0f }))
			{
				ComponentCommands::Remove(_ctx, &_object, _typeInfo);
			}
		}
	}

	ui.GetIdStack().Pop();
}

// -------------------------------------------------------------------------------
// コンポーネントの追加
//
// 一覧は ComponentRegistry から作る
// すでに持っている型は選べないようにして、押しても何も起きない項目を無くす
// -------------------------------------------------------------------------------
void Editor::InspectorPanel::DrawAddComponentMenu(EditorContext& _ctx, GameObject& _object)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	if (EditorUI::Button(ui, "コンポーネントを追加", font, { 200.0f, 24.0f }))
	{
		// 押したボタンの真下に開き、どのボタンから出たメニューかを分かりやすくする
		if (const EditorUI::WindowFrame* pFrame = ui.GetCurrentWindow())
		{
			ui.OpenPopupAt(kAddComponentMenu,
				{ pFrame->LastItemRect.Min.x, pFrame->LastItemRect.Max.y });
		}
	}

	if (EditorUI::BeginPopup(ui, kAddComponentMenu))
	{
		for (const ComponentTypeInfo& info : ComponentRegistry::GetAll())
		{
			const bool alreadyHas = (info.Has != nullptr && info.Has(_object));

			if (EditorUI::MenuItem(ui, font, info.DisplayName, !alreadyHas))
			{
				ComponentCommands::Add(_ctx, &_object, info);
			}
		}

		EditorUI::EndPopup(ui);
	}
}

// -------------------------------------------------------------------------------
// 選択中のアセットの情報
// -------------------------------------------------------------------------------
void Editor::InspectorPanel::DrawAssetInspector(EditorContext& _ctx)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	const std::filesystem::path& path = _ctx.pSelection->GetAssetPath();
	if (path.empty())
	{
		EditorUI::TextMuted(ui, font, "アセットが選択されていません");
		return;
	}

	const AssetType type = std::filesystem::is_directory(path)
		? AssetType::Folder
		: AssetDatabase::ClassifyPath(path);

	EditorUI::Text(ui, font, path.filename().string());
	EditorUI::TextMuted(ui, font, std::string(AssetDatabase::GetTypeLabel(type)));

	EditorUI::Separator(ui);

	// パスは長くなりがちなので、折り返して全体が読めるようにする
	EditorUI::TextMuted(ui, font, "Path");
	EditorUI::TextWrapped(ui, font, path.string());

	if (type != AssetType::Folder)
	{
		std::error_code error;
		const auto size = std::filesystem::file_size(path, error);

		if (!error)
		{
			EditorUI::TextMuted(ui, font, "Size : " + std::to_string(size) + " bytes");
		}
	}

	// -------------------------------------------------------------------------------
	// 開く操作
	// 種類ごとに「開いたときに何が起きるか」をここへ足していく
	// -------------------------------------------------------------------------------
	if (type == AssetType::Effect)
	{
		EditorUI::Separator(ui);

		if (EditorUI::Button(ui, "エフェクトエディタで開く", font, { 200.0f, 26.0f }))
		{
			EffectEditorPanel::OpenForAsset(_ctx, path);
		}
	}

	// -------------------------------------------------------------------------------
	// プレファブは「シーンへ置く」ことが開くことにあたる
	// -------------------------------------------------------------------------------
	if (type == AssetType::Prefab)
	{
		EditorUI::Separator(ui);

		const bool hasScene = (_ctx.pObjects != nullptr);

		if (EditorUI::Button(ui, "シーンへ配置", font, { 200.0f, 26.0f }) && hasScene)
		{
			PrefabSystem::InstantiateFromFile(_ctx, path);
		}

		if (!hasScene)
		{
			EditorUI::TextMuted(ui, font, "シーンが読み込まれていません");
		}
	}
}
