#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Editor/EditorContext.h>
#include <Editor/Assets/AssetDatabase.h>
#include <Editor/Panels/PanelManager/PanelManager.h>

namespace Editor
{
	class ViewportPanel;

	// -------------------------------------------------------------------------------
	// EditorApp class
	//
	// 概要 :
	//	エディタ全体をまとめる最上位のクラス
	//
	//	Applicationから見ると「毎フレームBuildUIを呼ぶだけ」の存在で、
	//	どんなパネルがあるか、どれが開いているかをApplicationは一切知らない
	//	これにより、Applicationはウィンドウとメインループの管理だけに戻れる
	//
	//	担当すること
	//		- エディタが持つ状態（選択・アセット・パネル一覧）の所有
	//		- 常設パネルの登録と、既定のレイアウト
	//		- 画面上部のメニューバー
	//		- 毎フレームのEditorContextの組み立て
	//
	//	担当しないこと
	//		- 個々のパネルの中身（各パネルのOnGUI）
	//		- 描画API（Applicationとレンダラの領分）
	// -------------------------------------------------------------------------------
	class EditorApp
	{
	public:

		// -------------------------------------------------------------------------------
		// @brief	初期化
		//
		// @param[in]	_pUI			ウィジェットの発行先
		// @param[in]	_pFont			文字の描画に使うフォント
		// @param[in]	_pViewport		ゲーム画面のレンダーターゲット
		// @param[in]	_pScenes		シーンの問い合わせ先
		// @param[in]	_contentRoot	コンテンツブラウザのルートフォルダ
		// -------------------------------------------------------------------------------
		bool Init(
			EditorUI::Context*				_pUI,
			EditorUI::Font*					_pFont,
			ViewportTarget*					_pViewport,
			SceneManager*					_pScenes,
			const std::filesystem::path&	_contentRoot);

		void Term();

		// -------------------------------------------------------------------------------
		// @brief	1フレーム分のUIを組み立てる
		//
		//	EditorUI::Context::NewFrame と EndFrame の間で呼ぶ
		//
		// @param[in]	_deltaTime	前フレームからの経過時間(秒)
		// -------------------------------------------------------------------------------
		void BuildUI(float _deltaTime);

		// -------------------------------------------------------------------------------
		// @brief	ゲーム画面として今フレームに必要な大きさ
		//
		//	Applicationがこれを見てレンダーターゲットを作り直し、
		//	シーンを描くかどうかを決める
		//
		// @param[out]	_outWidth	必要な横幅(ピクセル)
		// @param[out]	_outHeight	必要な縦幅(ピクセル)
		// @return	true : ゲーム画面を描く必要がある
		// -------------------------------------------------------------------------------
		bool GetRequestedViewportSize(uint32_t& _outWidth, uint32_t& _outHeight) const;

		// マウスがゲーム画面の上にあるか。カメラ操作を許すかの判断に使う
		bool IsViewportHovered() const;

	private:

		// 常設パネルを登録する
		void RegisterDefaultPanels();

		// -------------------------------------------------------------------------------
		// @brief	起動直後に一度だけ、UE5に似た既定のドッキングレイアウトを組む
		//
		//	画面サイズが確定してからでないと区画を割れないため、
		//	初期化時ではなく最初のBuildUIで行う
		// -------------------------------------------------------------------------------
		void BuildDefaultLayout();

		// 画面上部のメニューバー
		void DrawMenuBar();

		// 選択中オブジェクトがまだ生きているかを確認する
		void ValidateSelection();

		// 毎フレームのEditorContextを組み立てる
		void BuildContext(float _deltaTime);

		EditorContext	m_Context;
		PanelManager	m_Panels;
		Selection		m_Selection;
		AssetDatabase	m_Assets;

		// 常設パネルのうち、EditorAppが直接問い合わせるもの
		// 所有権はPanelManagerが持つため、ここでは参照だけを覚えておく
		ViewportPanel*	m_pViewportPanel = nullptr;

		EditorUI::Context*	m_pUI		= nullptr;
		EditorUI::Font*		m_pFont		= nullptr;
		ViewportTarget*		m_pViewport	= nullptr;
		SceneManager*		m_pScenes	= nullptr;

		bool m_Initialized = false;

		// 既定レイアウトを組んだか。組み直すとユーザーの配置を壊すため一度きりにする
		bool m_LayoutInitialized = false;
	};
}
