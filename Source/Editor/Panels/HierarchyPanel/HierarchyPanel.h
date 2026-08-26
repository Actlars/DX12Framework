#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Editor/Panels/IEditorPanel.h>

namespace Editor
{
	// -------------------------------------------------------------------------------
	// HierarchyPanel class
	//
	// 概要 :
	//	シーンに存在するGameObjectを一覧で表示し、選択するパネル
	//
	//	UE5のアウトライナに相当する
	//	表示しているのは実際のGameObjectManagerの中身そのもので、
	//	エディタ用の複製は持たない。そのため常にシーンの現状と一致する
	//
	//	選択結果はSelectionへ書き込むだけで、
	//	インスペクタへ直接は伝えない（パネル同士を疎結合に保つため）
	// -------------------------------------------------------------------------------
	class HierarchyPanel : public IEditorPanel
	{
	public:

		// -------------------------------------------------------------------------------
		// @brief	コンストラクタ
		//
		// @param[in]	_index	同じ種類の中での通し番号（1が最初の1枚）
		//						タイトルはEditorUIがウィンドウを識別するIdの元になるため、
		//						2枚目以降は番号を付けて必ず一意にする
		// -------------------------------------------------------------------------------
		explicit HierarchyPanel(int _index = 1);

		const std::string& GetTitle() const override { return m_Title; }

		// 通し番号を含まない名前。何枚開いているかを数えるのに使う
		std::string_view GetTypeName() const override { return "Hierarchy"; }

		void OnGUI(EditorContext& _ctx) override;

	private:

		// 検索欄に入力された文字で一覧を絞り込む
		bool MatchesFilter(std::string_view _name) const;

		// 右クリックメニュー（オブジェクトの作成・複製・削除）
		void DrawContextMenu(EditorContext& _ctx, GameObject* _pTarget);

		std::string m_Title;

		// 名前の一部で絞り込むための検索文字列
		std::string m_Filter;

		// 右クリックで対象にしたオブジェクト
		// メニューを閉じるまで覚えておく必要があるため、フレームをまたいで保持する
		GameObject* m_pContextTarget = nullptr;
	};
}
