// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "InspectorPanel.h"

#include <Engine/EditorUI/Widgets/Widgets.h>
#include <Engine/GameObject/GameObject.h>
#include <Engine/GameObject/Components/TransformComponent/TransformComponent.h>
#include <Editor/Assets/AssetDatabase.h>
#include <Editor/Panels/PanelManager/PanelManager.h>
#include <Editor/Panels/EffectEditorPanel/EffectEditorPanel.h>

namespace
{
	// インスペクタ全体で共有するレイアウト
	// 同じ値を使うことで、どの行もラベルの右端がそろう
	constexpr EditorUI::PropertyLayout kInspectorLayout{ 88.0f, 0.0f, 4.0f };
}

Editor::InspectorPanel::InspectorPanel()
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
// -------------------------------------------------------------------------------
void Editor::InspectorPanel::DrawObjectInspector(EditorContext& _ctx, GameObject& _object)
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
		_object.SetName(m_NameBuffer);
	}

	bool active = _object.IsActive();
	if (EditorUI::Property(ui, font, "Active", &active, kInspectorLayout))
	{
		_object.SetActive(active);
	}

	EditorUI::TextMuted(ui, font, "ID : " + std::to_string(_object.GetID()));

	EditorUI::Separator(ui);

	// -------------------------------------------------------------------------------
	// コンポーネント
	// 今はTransformのみ対応。ほかのコンポーネントも同じ形で足していける
	// -------------------------------------------------------------------------------
	DrawTransformSection(_ctx, _object);
}

// -------------------------------------------------------------------------------
// Transformの編集
//
// GameObjectが実際に持っている値を直接読み書きするため、
// 編集した結果はそのままシーンの見た目に反映される
// -------------------------------------------------------------------------------
void Editor::InspectorPanel::DrawTransformSection(EditorContext& _ctx, GameObject& _object)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	auto* pTransform = _object.GetComponent<TransformComponent>();
	if (pTransform == nullptr)
	{
		EditorUI::TextMuted(ui, font, "TransformComponent がありません");
		return;
	}

	if (!EditorUI::CollapsingHeader(ui, font, "Transform"))
	{
		return;
	}

	// -------------------------------------------------------------------------------
	// コンポーネント側はSet経由でしか書き換えられないため、
	// いったんローカルへ取り出し、変化があったときだけ書き戻す
	// -------------------------------------------------------------------------------
	DirectX::XMFLOAT3 position	= pTransform->GetPosition();
	DirectX::XMFLOAT3 rotation	= pTransform->GetRotation();
	DirectX::XMFLOAT3 scale		= pTransform->GetScale();

	if (EditorUI::Property(ui, font, "Position", &position, {}, kInspectorLayout))
	{
		pTransform->SetPosition(position);
	}

	// 回転は角度なので、-360～360に制限しつつ少し速めに動かす
	EditorUI::NumericEditorOptions<float> rotationOptions;
	rotationOptions.Min			= -360.0f;
	rotationOptions.Max			=  360.0f;
	rotationOptions.DragSpeed	= 0.5L;
	rotationOptions.Precision	= 1;
	rotationOptions.Step		= 0.0f;		// 丸めずに、そのままの値を扱う

	if (EditorUI::Property(ui, font, "Rotation", &rotation, rotationOptions, kInspectorLayout))
	{
		pTransform->SetRotation(rotation);
	}

	// スケールは0以下にすると表示が潰れるため、下限を設ける
	EditorUI::NumericEditorOptions<float> scaleOptions;
	scaleOptions.Min		= 0.01f;
	scaleOptions.Max		= 100.0f;
	scaleOptions.DragSpeed	= 0.01L;
	scaleOptions.Step		= 0.0f;

	if (EditorUI::Property(ui, font, "Scale", &scale, scaleOptions, kInspectorLayout))
	{
		pTransform->SetScale(scale);
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
}
