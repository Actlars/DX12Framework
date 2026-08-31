// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "ComponentInspectors.h"

#include <Engine/EditorUI/Widgets/Widgets.h>
#include <Engine/GameObject/GameObject.h>
#include <Engine/GameObject/Components/TransformComponent/TransformComponent.h>
#include <Engine/GameObject/Components/MeshComponent/MeshComponent.h>
#include <Engine/GameObject/Components/MeshletComponent/MeshletComponent.h>

#include <Editor/Commands/ObjectCommands/ObjectCommands.h>
#include <Editor/EditorContext.h>

namespace
{
	using namespace Editor;

	// どの行もラベルの右端がそろうよう、インスペクタ全体で同じレイアウトを使う
	constexpr EditorUI::PropertyLayout kInspectorLayout{ 88.0f, 0.0f, 4.0f };

	// -------------------------------------------------------------------------------
	// TransformEditSession struct
	//
	// 概要 :
	//	ドラッグ1回ぶんの編集を、1回のUndoにまとめるための覚え書き
	//
	//	ドラッグ中は毎フレーム値が変わるため、変化のたびに履歴へ積むと
	//	1回動かしただけで何十回もUndoが必要になってしまう
	//	そこで
	//		・変化し始めた最初のフレームで「編集前の値」を控える
	//		・どのウィジェットも掴まれていない状態になったら、1回だけ履歴へ積む
	//	という形にしている
	//
	//	ファイル内のstaticにしているのは、同時に編集できるウィジェットが
	//	常に1つだけだから（EditorUIのTextEditStateと同じ考え方）
	//	即時モードのウィジェットは自分で状態を持てないため、置き場所がここになる
	// -------------------------------------------------------------------------------
	struct TransformEditSession
	{
		const GameObject*	pObject = nullptr;	// 編集中の対象。nullptrなら編集していない
		TransformValues		Before;				// 編集を始める前の値
	};

	TransformEditSession s_TransformEdit;
}

// -------------------------------------------------------------------------------
// Transform
// -------------------------------------------------------------------------------
void Editor::ComponentInspectors::DrawTransform(EditorContext& _ctx, GameObject& _object)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	auto* pTransform = _object.GetComponent<TransformComponent>();
	if (pTransform == nullptr)
	{
		EditorUI::TextMuted(ui, font, "TransformComponent がありません");
		return;
	}

	// -------------------------------------------------------------------------------
	// 今の値を取り出し、変化があったときだけ書き戻す
	//
	// beforeThisFrameは、この行を触る前の値
	// ドラッグが始まった最初のフレームで、これが「編集前の値」になる
	// -------------------------------------------------------------------------------
	TransformValues			values			= ObjectCommands::ReadTransform(_object);
	const TransformValues	beforeThisFrame	= values;

	bool changed = false;

	changed |= EditorUI::Property(ui, font, "Position", &values.Position, {}, kInspectorLayout);

	// 回転は角度なので、-360～360に制限しつつ少し速めに動かす
	EditorUI::NumericEditorOptions<float> rotationOptions;
	rotationOptions.Min			= -360.0f;
	rotationOptions.Max			=  360.0f;
	rotationOptions.DragSpeed	= 0.5L;
	rotationOptions.Precision	= 1;
	rotationOptions.Step		= 0.0f;		// 丸めずに、そのままの値を扱う

	changed |= EditorUI::Property(ui, font, "Rotation", &values.Rotation, rotationOptions, kInspectorLayout);

	// スケールは0以下にすると表示が潰れるため、下限を設ける
	EditorUI::NumericEditorOptions<float> scaleOptions;
	scaleOptions.Min		= 0.01f;
	scaleOptions.Max		= 100.0f;
	scaleOptions.DragSpeed	= 0.01L;
	scaleOptions.Step		= 0.0f;

	changed |= EditorUI::Property(ui, font, "Scale", &values.Scale, scaleOptions, kInspectorLayout);

	// -------------------------------------------------------------------------------
	// 変化はその場で反映する
	// 履歴へ積むのはあとなので、画面の見た目は遅れずに追従する
	// -------------------------------------------------------------------------------
	if (changed)
	{
		// 編集の始まりなら、この時点の値を「編集前」として控える
		if (s_TransformEdit.pObject != &_object)
		{
			s_TransformEdit.pObject	= &_object;
			s_TransformEdit.Before	= beforeThisFrame;
		}

		ObjectCommands::ApplyTransform(_object, values);
	}

	// -------------------------------------------------------------------------------
	// 編集の終わり
	//
	// どのウィジェットも掴まれていない = ドラッグを離した、または入力を確定した
	// このタイミングで、ドラッグ全体をまとめて1回だけ履歴へ積む
	// -------------------------------------------------------------------------------
	if (s_TransformEdit.pObject == &_object && !ui.IsAnyItemActive())
	{
		ObjectCommands::SetTransform(_ctx, &_object, s_TransformEdit.Before, values);

		s_TransformEdit = {};
	}
}

// -------------------------------------------------------------------------------
// Mesh
//
// メッシュそのものの割り当てはシーン側が行うため、
// ここでは「入っているか」と「表示するか」だけを扱う
// -------------------------------------------------------------------------------
void Editor::ComponentInspectors::DrawMesh(EditorContext& _ctx, GameObject& _object)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	auto* pMesh = _object.GetComponent<MeshComponent>();
	if (pMesh == nullptr)
	{
		return;
	}

	if (!pMesh->HasMesh())
	{
		// 表示フラグを出しても意味が無い状態なので、理由だけを示す
		EditorUI::TextMuted(ui, font, "メッシュが未設定です");
		EditorUI::TextMuted(ui, font, "モデルの割り当てはシーン側で行われます");
		return;
	}

	bool visible = pMesh->IsVisible();
	if (EditorUI::Property(ui, font, "Visible", &visible, kInspectorLayout))
	{
		pMesh->SetVisible(visible);
	}
}

// -------------------------------------------------------------------------------
// Meshlet
// -------------------------------------------------------------------------------
void Editor::ComponentInspectors::DrawMeshlet(EditorContext& _ctx, GameObject& _object)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	auto* pMeshlet = _object.GetComponent<MeshletComponent>();
	if (pMeshlet == nullptr)
	{
		return;
	}

	const size_t meshCount = pMeshlet->GetMeshCount();

	EditorUI::TextMuted(ui, font, "Meshes : " + std::to_string(meshCount));

	if (meshCount == 0)
	{
		EditorUI::TextMuted(ui, font, "モデルが未読み込みです");
		return;
	}

	bool visible = pMeshlet->IsVisible();
	if (EditorUI::Property(ui, font, "Visible", &visible, kInspectorLayout))
	{
		pMeshlet->SetVisible(visible);
	}
}
