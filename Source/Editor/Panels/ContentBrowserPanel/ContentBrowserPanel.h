#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Editor/Panels/IEditorPanel.h>
#include <Editor/Assets/AssetDatabase.h>

namespace Editor
{
	// -------------------------------------------------------------------------------
	// ContentBrowserPanel class
	//
	// 概要 :
	//	コンテンツフォルダの中身を閲覧・作成・削除するパネル
	//
	//	UE5のコンテンツブラウザに相当する
	//	ファイル操作そのものはAssetDatabaseが持ち、このクラスは
	//	「どう見せて、どの操作をどのメニューに置くか」だけを担当する
	//
	//	操作
	//		クリック		… 選択（インスペクタに情報が出る）
	//		ダブルクリック	… フォルダなら移動、.effectならエフェクトエディタで開く
	//		右クリック		… 新規フォルダ / 新規ファイル / 新規エフェクト / 削除
	// -------------------------------------------------------------------------------
	class ContentBrowserPanel : public IEditorPanel
	{
	public:

		// -------------------------------------------------------------------------------
		// @brief	コンストラクタ
		//
		// @param[in]	_index	同じ種類の中での通し番号（1が最初の1枚）
		//						タイトルはEditorUIがウィンドウを識別するIdの元になるため、
		//						2枚目以降は番号を付けて必ず一意にする
		// -------------------------------------------------------------------------------
		explicit ContentBrowserPanel(int _index = 1);

		const std::string& GetTitle() const override { return m_Title; }

		// 通し番号を含まない名前。何枚開いているかを数えるのに使う
		std::string_view GetTypeName() const override { return "Content Browser"; }

		void OnGUI(EditorContext& _ctx) override;

	private:

		// ルートからの現在位置を示す、クリックで戻れるパンくずリスト
		void DrawBreadcrumb(EditorContext& _ctx);

		// 項目一覧。1行1エントリで表示する
		void DrawEntries(EditorContext& _ctx);

		// エントリを「開く」(ダブルクリック時の動作)
		void OpenEntry(EditorContext& _ctx, const AssetEntry& _entry);

		// 右クリックメニューの中身
		// _pTargetがnullptrなら、何もない場所を右クリックした場合
		void DrawContextMenu(EditorContext& _ctx, const AssetEntry* _pTarget);

		// 新規作成の共通処理。作ったものをそのまま選択状態にする
		void CreateNewFolder(EditorContext& _ctx);
		void CreateNewTextFile(EditorContext& _ctx);
		void CreateNewEffect(EditorContext& _ctx, bool _openEditor);

		std::string m_Title;

		// 右クリックされたエントリ。メニューを閉じるまで覚えておく
		AssetEntry	m_ContextTarget;
		bool		m_HasContextTarget = false;
	};
}
