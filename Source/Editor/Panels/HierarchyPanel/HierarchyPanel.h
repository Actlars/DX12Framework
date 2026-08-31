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

		// 1行ぶんの表示。名前変更中はその行だけ入力欄に差し替わる
		void DrawObjectRow(EditorContext& _ctx, GameObject& _object, bool& _outRightClicked);

		// 名前変更中の行（入力欄）
		void DrawRenameRow(EditorContext& _ctx, GameObject& _object);

		// 名前の変更を始める / やめる
		void BeginRename(GameObject& _object);
		void EndRename();

		// 右クリックメニュー（オブジェクトの作成・複製・名前変更・削除）
		void DrawContextMenu(EditorContext& _ctx, GameObject* _pTarget);

		std::string m_Title;

		// 名前の一部で絞り込むための検索文字列
		std::string m_Filter;

		// 右クリックで対象にしたオブジェクト
		// メニューを閉じるまで覚えておく必要があるため、フレームをまたいで保持する
		GameObject* m_pContextTarget = nullptr;

		// -------------------------------------------------------------------------------
		// 名前の変更
		//
		//	対象の行だけを入力欄に差し替えて、その場で打ち直せるようにする
		//	（別のウィンドウを開くより、どれを変更しているかが分かりやすい）
		//
		//	m_RenameActivateは「開始した最初の1フレームだけ入力欄へ移る」ための合図
		//	毎フレーム立てたままにすると、確定しても入力状態へ戻ってしまう
		// -------------------------------------------------------------------------------
		GameObject* m_pRenameTarget		= nullptr;
		std::string m_RenameBuffer;
		bool		m_RenameActivate	= false;
	};
}
