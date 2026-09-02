// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "ComponentInspectors.h"

#include <Engine/EditorUI/Widgets/Widgets.h>
#include <Engine/GameObject/GameObject.h>
#include <Engine/GameObject/ComponentRegistry/ComponentRegistry.h>
#include <Engine/GameObject/Components/TransformComponent/TransformComponent.h>
#include <Engine/GameObject/Components/MeshComponent/MeshComponent.h>
#include <Engine/GameObject/Components/MeshletComponent/MeshletComponent.h>
#include <Engine/GameObject/Components/HitComponent/BoxCollisionComponent/BoxCollisionComponent.h>
#include <Engine/Resource/ModelLibrary/ModelLibrary.h>

#include <Editor/Commands/ComponentCommands/ComponentCommands.h>
#include <Editor/Commands/ObjectCommands/ObjectCommands.h>
#include <Editor/EditorContext.h>

namespace
{
	using namespace Editor;

	// どの行もラベルの右端がそろうよう、インスペクタ全体で同じレイアウトを使う
	constexpr EditorUI::PropertyLayout kInspectorLayout{ 88.0f, 0.0f, 4.0f };

	// モデル選択メニューの名前。Idの元になるため一意にする
	constexpr std::string_view kModelPickerMenu = "ComponentModelPicker";

	// モデルとして扱う拡張子
	constexpr std::string_view kModelExtensions[] = { ".fbx", ".obj", ".gltf", ".glb" };

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

	// -------------------------------------------------------------------------------
	// 選べるモデルの一覧
	//
	//	メニューを開いた瞬間だけ作り直す
	//	毎フレーム走査するとフォルダの中身を何度も読むことになり、
	//	かといって起動時に1度だけでは、あとから足したモデルが出てこない
	// -------------------------------------------------------------------------------
	std::vector<std::string> s_ModelCandidates;

	bool IsModelFile(const std::filesystem::path& _path)
	{
		if (!_path.has_extension())
		{
			return false;
		}

		std::string extension = _path.extension().string();

		std::transform(extension.begin(), extension.end(), extension.begin(),
			[](unsigned char _c) { return static_cast<char>(std::tolower(_c)); });

		return std::find(std::begin(kModelExtensions), std::end(kModelExtensions), extension)
			!= std::end(kModelExtensions);
	}

	// -------------------------------------------------------------------------------
	// プロジェクトのAssetsフォルダからモデルファイルを集める
	//
	// 読み込み済みのものも混ぜる
	// （Assetsの外にあるモデルを一度でも使っていれば、選び直せるようにするため）
	// -------------------------------------------------------------------------------
	void RebuildModelCandidates(const ModelLibrary& _models)
	{
		s_ModelCandidates = _models.GetLoadedKeys();

		const std::filesystem::path assetsRoot = _models.GetProjectRoot() / "Assets";

		std::error_code error;
		if (std::filesystem::exists(assetsRoot, error) && !error)
		{
			for (const auto& entry : std::filesystem::recursive_directory_iterator(
					assetsRoot, std::filesystem::directory_options::skip_permission_denied, error))
			{
				if (error)
				{
					break;
				}

				if (!entry.is_regular_file() || !IsModelFile(entry.path()))
				{
					continue;
				}

				s_ModelCandidates.emplace_back(_models.ToKey(entry.path()));
			}
		}

		// 同じものが二重に並ばないようにする
		std::sort(s_ModelCandidates.begin(), s_ModelCandidates.end());
		s_ModelCandidates.erase(
			std::unique(s_ModelCandidates.begin(), s_ModelCandidates.end()),
			s_ModelCandidates.end());
	}

	// -------------------------------------------------------------------------------
	// モデルを選ぶボタンとメニュー
	//
	//	選ばれたモデルのキーを _outSelected へ入れてtrueを返す
	//	「指定を外す」を選んだ場合は空文字が入る
	//
	//	ここでは値を書き換えない
	//	書き換え方（Meshか、Meshletか）は呼び出し側の型ごとに違うため
	// -------------------------------------------------------------------------------
	bool DrawModelPicker(
		EditorContext&		_ctx,
		std::string_view	_currentKey,
		std::string&		_outSelected)
	{
		EditorUI::Context&	ui		= *_ctx.pUI;
		EditorUI::Font&		font	= *_ctx.pFont;

		if (_ctx.pModels == nullptr)
		{
			EditorUI::TextMuted(ui, font, "モデルの置き場がありません");
			return false;
		}

		// -------------------------------------------------------------------------------
		// 今の指定を表示する
		//
		// パスは長くなりがちなので、折り返して全体が読めるようにする
		// -------------------------------------------------------------------------------
		EditorUI::TextMuted(ui, font, "Model");

		if (_currentKey.empty())
		{
			EditorUI::TextMuted(ui, font, "（未設定）");
		}
		else
		{
			EditorUI::TextWrapped(ui, font, std::string(_currentKey));
		}

		if (EditorUI::Button(ui, "モデルを選択", font, { 160.0f, 22.0f }))
		{
			// 開いた瞬間に一覧を作り直す
			RebuildModelCandidates(*_ctx.pModels);

			// 押したボタンの真下に開き、どこから出たメニューかを分かりやすくする
			if (const EditorUI::WindowFrame* pFrame = ui.GetCurrentWindow())
			{
				ui.OpenPopupAt(kModelPickerMenu,
					{ pFrame->LastItemRect.Min.x, pFrame->LastItemRect.Max.y });
			}
		}

		bool selected = false;

		if (EditorUI::BeginPopup(ui, kModelPickerMenu))
		{
			if (EditorUI::MenuItem(ui, font, "（指定を外す）"))
			{
				_outSelected.clear();
				selected = true;
			}

			EditorUI::MenuSeparator(ui);

			if (s_ModelCandidates.empty())
			{
				EditorUI::MenuItem(ui, font, "モデルが見つかりません", false);
			}

			for (const std::string& candidate : s_ModelCandidates)
			{
				// 選択中のものには印を付け、今どれが使われているか分かるようにする
				const std::string label = (candidate == _currentKey ? "* " : "  ") + candidate;

				if (EditorUI::MenuItem(ui, font, label))
				{
					_outSelected	= candidate;
					selected		= true;
				}
			}

			EditorUI::EndPopup(ui);
		}

		return selected;
	}

	// -------------------------------------------------------------------------------
	// コンポーネントの値の変更を、取り消せる操作として履歴へ積む
	//
	//	書き換え前後の値をComponentRegistry経由で写し取り、そのまま渡すだけ
	//	型ごとに専用のコマンドを作らずに済む
	// -------------------------------------------------------------------------------
	void SubmitValueChange(
		EditorContext&				_ctx,
		GameObject&					_object,
		std::string_view			_typeName,
		const nlohmann::json&		_before)
	{
		const ComponentTypeInfo* pInfo = ComponentRegistry::Find(_typeName);

		if (pInfo == nullptr)
		{
			return;
		}

		ComponentCommands::SetValues(
			_ctx, &_object, *pInfo,
			_before,
			ComponentRegistry::CaptureComponent(_object, *pInfo));
	}
}

// -------------------------------------------------------------------------------
// 型名に対応する見せ方
// -------------------------------------------------------------------------------
const Editor::ComponentDisplayInfo& Editor::ComponentInspectors::GetDisplay(std::string_view _typeName)
{
	// -------------------------------------------------------------------------------
	// 見せ方の登録表
	//
	//	新しいコンポーネントをエディタで扱いやすくするときは、ここへ1項目足す
	//	足さなくても表示はできる（型名がそのまま出る）ため、
	//	エンジン側の登録とどちらが先でも構わない
	// -------------------------------------------------------------------------------
	static const std::vector<ComponentDisplayInfo> s_Table =
	{
		{ "TransformComponent",		"Transform",	false,	&DrawTransform		},
		{ "MeshComponent",			"Mesh",			true,	&DrawMesh			},
		{ "MeshletComponent",		"Meshlet",		true,	&DrawMeshlet		},
		{ "BoxCollisionComponent",	"BoxCollision", false,	&DrawBoxCollision	},
	};

	for (const ComponentDisplayInfo& info : s_Table)
	{
		if (info.TypeName == _typeName)
		{
			return info;
		}
	}

	// -------------------------------------------------------------------------------
	// 登録が無い型のための既定
	//
	// 表示名に型名をそのまま使う
	// staticで持つのは、参照を返すため寿命が必要になるから
	// -------------------------------------------------------------------------------
	static ComponentDisplayInfo s_Fallback;

	s_Fallback.TypeName		= _typeName;
	s_Fallback.DisplayName	= _typeName;
	s_Fallback.IsRemovable	= true;
	s_Fallback.Draw			= nullptr;

	return s_Fallback;
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
// 描くモデルをここで選べる
// 選んだ時点では見た目は変わらず、次のフレームにシーンが実体を結び付ける
// （MeshComponentのコメントを参照）
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

	const ComponentTypeInfo* pInfo = ComponentRegistry::Find("MeshComponent");
	if (pInfo == nullptr)
	{
		return;
	}

	// -------------------------------------------------------------------------------
	// モデルの選択
	// -------------------------------------------------------------------------------
	std::string selectedKey;

	if (DrawModelPicker(_ctx, pMesh->GetModelKey(), selectedKey))
	{
		const nlohmann::json before = ComponentRegistry::CaptureComponent(_object, *pInfo);

		// 別のモデルへ移ったら、パート番号は先頭へ戻す
		// 前のモデルの番号をそのまま使うと、範囲外になって何も出なくなる
		pMesh->SetModelRequest(selectedKey, 0);

		SubmitValueChange(_ctx, _object, "MeshComponent", before);
	}

	// -------------------------------------------------------------------------------
	// モデル内の何番目を描くか
	//
	// 1つのモデルファイルは、体・髪・服のように複数のメッシュに分かれていることが多い
	// MeshComponentが描くのはそのうちの1つなので、番号で選ぶ
	// -------------------------------------------------------------------------------
	if (!pMesh->GetModelKey().empty())
	{
		uint32_t part = pMesh->GetPartIndex();

		EditorUI::NumericEditorOptions<uint32_t> partOptions;
		partOptions.Min			= 0;
		partOptions.Max			= 255;
		partOptions.DragSpeed	= 0.1L;

		if (EditorUI::Property(ui, font, "Part", &part, partOptions, kInspectorLayout))
		{
			const nlohmann::json before = ComponentRegistry::CaptureComponent(_object, *pInfo);

			pMesh->SetModelRequest(pMesh->GetModelKey(), part);

			SubmitValueChange(_ctx, _object, "MeshComponent", before);
		}
	}

	// -------------------------------------------------------------------------------
	// 反映状況
	//
	// 選んだ直後はまだ結び付いていないため、そのことを見えるようにする
	// -------------------------------------------------------------------------------
	if (pMesh->GetModelKey().empty())
	{
		EditorUI::TextMuted(ui, font, "モデルが未設定です");
		return;
	}

	if (!pMesh->HasMesh())
	{
		EditorUI::TextMuted(ui, font, "読み込み待ち、またはパート番号が範囲外です");
		return;
	}

	bool visible = pMesh->IsVisible();
	if (EditorUI::Property(ui, font, "Visible", &visible, kInspectorLayout))
	{
		const nlohmann::json before = ComponentRegistry::CaptureComponent(_object, *pInfo);

		pMesh->SetVisible(visible);

		SubmitValueChange(_ctx, _object, "MeshComponent", before);
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

	const ComponentTypeInfo* pInfo = ComponentRegistry::Find("MeshletComponent");
	if (pInfo == nullptr)
	{
		return;
	}

	std::string selectedKey;

	if (DrawModelPicker(_ctx, pMeshlet->GetModelKey(), selectedKey))
	{
		const nlohmann::json before = ComponentRegistry::CaptureComponent(_object, *pInfo);

		pMeshlet->SetModelRequest(selectedKey);

		SubmitValueChange(_ctx, _object, "MeshletComponent", before);
	}

	// メッシュレットはモデル全体をまとめて描くため、パート番号は無い
	EditorUI::TextMuted(ui, font, "Meshes : " + std::to_string(pMeshlet->GetMeshCount()));

	if (pMeshlet->GetMeshCount() == 0)
	{
		EditorUI::TextMuted(ui, font, "モデルが未読み込みです");
		return;
	}

	bool visible = pMeshlet->IsVisible();
	if (EditorUI::Property(ui, font, "Visible", &visible, kInspectorLayout))
	{
		const nlohmann::json before = ComponentRegistry::CaptureComponent(_object, *pInfo);

		pMeshlet->SetVisible(visible);

		SubmitValueChange(_ctx, _object, "MeshletComponent", before);
	}
}

// -------------------------------------------------------------------------------
// BoxCollision
// -------------------------------------------------------------------------------
void Editor::ComponentInspectors::DrawBoxCollision(EditorContext& _ctx, GameObject& _object)
{
	EditorUI::Context& ui = *_ctx.pUI;
	EditorUI::Font& font = *_ctx.pFont;

	auto* pBoxCollision = _object.GetComponent<BoxCollisionComponent>();

	if (pBoxCollision == nullptr)
	{ return; }

	const ComponentTypeInfo* pInfo = ComponentRegistry::Find("BoxCollisionComponent");
	if (pInfo == nullptr) 
	{ return; }

	// -------------------------------------------------------------------------------
	// Size
	// 
	// BoxCollisionでは、HalfExtentsとして保持している
	// Inspectorでは実際のBoxサイズとして扱うため、2倍して使う
	// -------------------------------------------------------------------------------
	const AABB& localAABB = pBoxCollision->GetLocalAABB();

	Math::Vector3 size
	{
		localAABB.HalfExtents.x * 2.0f,
		localAABB.HalfExtents.y * 2.0f,
		localAABB.HalfExtents.z * 2.0f
	};

	EditorUI::NumericEditorOptions<float> sizeOptions;

	sizeOptions.Min = 0.001f;
	sizeOptions.Max = 10000.0f;
	sizeOptions.DragSpeed = 0.01L;
	sizeOptions.Precision = 3;
	sizeOptions.Step = 0.0f;

	if (EditorUI::Property(ui, font, "Size", &size, sizeOptions, kInspectorLayout))
	{
		const nlohmann::json before = ComponentRegistry::CaptureComponent(_object, *pInfo);
		pBoxCollision->SetSize(Math::Vector3{ size.x * 0.5f,size.y * 0.5f,size.z * 0.5f });
		SubmitValueChange(_ctx, _object, "BoxCollisionComponent", before);
	}

	// -------------------------------------------------------------------------------
	// Offset
	// -------------------------------------------------------------------------------
	Math::Vector3 offset
	{
		localAABB.Center.x,
		localAABB.Center.y,
		localAABB.Center.z
	};

	EditorUI::NumericEditorOptions<float> offsetOptions;

	offsetOptions.Min = -100000.0f;
	offsetOptions.Max = 100000.0f;
	offsetOptions.DragSpeed = 0.01f;
	offsetOptions.Precision = 3;
	offsetOptions.Step = 0.0f;

	if (EditorUI::Property(ui, font, "Offset", &offset, offsetOptions, kInspectorLayout))
	{
		const nlohmann::json before = ComponentRegistry::CaptureComponent(_object, *pInfo);
		pBoxCollision->SetOffset(offset);
		SubmitValueChange(_ctx, _object, "BoxCollisionComponent", before);
	}

	// -------------------------------------------------------------------------------
	// DebugDraw
	// -------------------------------------------------------------------------------
	bool debugDraw = pBoxCollision->IsDebugDrawEnabled();

	if (EditorUI::Property(ui, font, "DebugDraw", &debugDraw, kInspectorLayout))
	{
		const nlohmann::json before = ComponentRegistry::CaptureComponent(_object, *pInfo);
		pBoxCollision->SetDebugDrawEnabled(debugDraw);
		SubmitValueChange(_ctx, _object, "BoxCollisionComponent", before);
	}

	// -------------------------------------------------------------------------------
	// DebugColor
	// -------------------------------------------------------------------------------
	const Math::Vector4& currentColor = pBoxCollision->GetDebugColor();

	Math::Vector4 color = currentColor;

	EditorUI::NumericEditorOptions<float> colorOptions;

	colorOptions.Min = 0.0f;
	colorOptions.Max = 1.0f;
	colorOptions.DragSpeed = 0.01L;
	colorOptions.Precision = 2;
	colorOptions.Step = 0.0f;

	if (EditorUI::Property(ui, font, "DebugColor", &color, colorOptions, kInspectorLayout))
	{
		const nlohmann::json before = ComponentRegistry::CaptureComponent(_object, *pInfo);
		pBoxCollision->SetDebugColor(color);
		SubmitValueChange(_ctx, _object, "BoxCollisionComponent", before);
	}

}
