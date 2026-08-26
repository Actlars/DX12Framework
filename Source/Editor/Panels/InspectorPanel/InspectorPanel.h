#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Editor/Panels/IEditorPanel.h>

class GameObject;

namespace Editor
{
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
	//		SceneObject	… 名前・アクティブ・Transformなどのコンポーネント
	//		Asset		… ファイルの情報と、開くための操作
	//		None		… 何も選ばれていないことの案内
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

		// TransformComponentを持っていれば、その値を編集できるようにする
		void DrawTransformSection(EditorContext& _ctx, GameObject& _object);

		// 選択中のアセットの情報を並べる
		void DrawAssetInspector(EditorContext& _ctx);

		std::string m_Title;

		// 名前の編集用。GameObject::GetNameは参照を返すが、
		// InputTextは書き換え可能なstd::stringを要求するため、いったんここへ写して使う
		std::string	m_NameBuffer;
		uint64_t	m_NameBufferOwner = 0;	// どのオブジェクトの名前を写しているか
	};
}
