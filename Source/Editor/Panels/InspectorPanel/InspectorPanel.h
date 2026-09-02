#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Editor/Panels/IEditorPanel.h>

class GameObject;
struct ComponentTypeInfo;

namespace Editor
{
	struct ComponentDisplayInfo;

	// -------------------------------------------------------------------------------
	// InspectorPanel class
	//
	// 概要 :
	//	いま選ばれているものの詳細を表示・編集するパネル
	//
	//	自分では何も選ばない
	//	Selectionを読み、その種類に応じて表示を切り替えるだけに徹する
	//	これにより「選ぶ側」のパネルが増えても、このパネルは変更しなくてよい
	//
	//	表示の分岐
	//		SceneObject	… 名前・アクティブ・持っているコンポーネント
	//		Asset		… ファイルの情報と、開くための操作
	//		None		… 何も選ばれていないことの案内
	//
	//	コンポーネントの中身は自分で描かない
	//	どの型をどう見せるかは ComponentRegistry / ComponentInspectors が持ち、
	//	このパネルは見出し・折りたたみ・削除ボタンといった、
	//	どの型でも共通する枠だけを描く
	//	そのため、新しいコンポーネントが増えてもこのファイルは変更不要になる
	// -------------------------------------------------------------------------------
	class InspectorPanel : public IEditorPanel
	{
	public:

		// -------------------------------------------------------------------------------
		// @brief	コンストラクタ
		//
		// @param[in]	_index	同じ種類の中での通し番号（1が最初の1枚）
		//						タイトルはEditorUIがウィンドウを識別するIdの元になるため、
		//						2枚目以降は番号を付けて必ず一意にする
		// -------------------------------------------------------------------------------
		explicit InspectorPanel(int _index = 1);

		const std::string& GetTitle() const override { return m_Title; }

		// 通し番号を含まない名前。何枚開いているかを数えるのに使う
		std::string_view GetTypeName() const override { return "Inspector"; }

		void OnGUI(EditorContext& _ctx) override;

	private:

		// GameObjectの中身を並べる
		void DrawObjectInspector(EditorContext& _ctx, GameObject& _object);

		// 名前・アクティブ・IDといった、コンポーネント以前の基本情報
		void DrawObjectHeader(EditorContext& _ctx, GameObject& _object);

		// 持っているコンポーネントを、登録順に並べる
		void DrawComponentSections(EditorContext& _ctx, GameObject& _object);

		// -------------------------------------------------------------------------------
		// @brief	コンポーネント1つぶんの枠を描く
		//
		//	見出し・折りたたみ・削除ボタンまでがここの担当
		//	中身は _typeInfo が指すインスペクタ関数へ任せる
		// -------------------------------------------------------------------------------
		void DrawComponentSection(
			EditorContext&					_ctx,
			GameObject&						_object,
			const ComponentTypeInfo&		_typeInfo,
			const ComponentDisplayInfo&		_display);

		// 「コンポーネントを追加」ボタンと、その中身
		void DrawAddComponentMenu(EditorContext& _ctx, GameObject& _object);

		// 選択中のアセットの情報を並べる
		void DrawAssetInspector(EditorContext& _ctx);

		std::string m_Title;

		// -------------------------------------------------------------------------------
		// 名前の編集用
		//
		//	GameObject::GetNameは参照を返すが、InputTextは書き換え可能な
		//	std::stringを要求するため、いったんここへ写して使う
		//	選択が変わったときだけ写し直す（毎フレーム写すと入力中の文字が消える）
		// -------------------------------------------------------------------------------
		std::string	m_NameBuffer;
		uint64_t	m_NameBufferOwner = 0;	// どのオブジェクトの名前を写しているか
	};
}
