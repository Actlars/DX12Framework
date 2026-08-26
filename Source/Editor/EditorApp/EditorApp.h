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
			RHI::Device*					_pDevice,
			EditorUIRenderer*				_pUIRenderer,
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

		// -------------------------------------------------------------------------------
		// @brief	自前のGPU描画を持つパネル（エフェクトのプレビュー等）を描く
		//
		//	BuildUIのあと、ゲーム画面やUIをバックバッファへ描く前に呼ぶ
		//
		// @param[in]	_pCmd	記録中のコマンドリスト
		// -------------------------------------------------------------------------------
		void RenderPanels(ID3D12GraphicsCommandList* _pCmd);

		// マウスがゲーム画面の上にあるか。カメラ操作を許すかの判断に使う
		bool IsViewportHovered() const;

	private:

		// -------------------------------------------------------------------------------
		// PanelFactory struct
		//
		// 概要 :
		//	「同じ種類のウィンドウをもう1枚開く」ための作り方
		//
		//	メニューへ項目を並べるのも、上限を判定するのもこの一覧から行うため、
		//	新しく複数開けるパネルを足すときは、ここへ1行加えるだけで済む
		// -------------------------------------------------------------------------------
		struct PanelFactory
		{
			std::string TypeName;		// 種類名（IEditorPanel::GetTypeName と一致させる）
			std::string MenuLabel;		// メニューに出す表示名

			// 通し番号を受け取り、そのぶんのパネルを作る
			std::function<std::unique_ptr<IEditorPanel>(int)> Create;

			// 同時に開ける最大数。増やしすぎて画面が埋まるのを防ぐ
			int MaxInstances = 4;
		};

		// 常設パネルを登録する
		void RegisterDefaultPanels();

		// 複数開けるパネルの作り方を登録する
		void RegisterPanelFactories();

		// -------------------------------------------------------------------------------
		// @brief	指定した種類のパネルをもう1枚開く
		//
		//	上限に達している場合は何もしない
		// -------------------------------------------------------------------------------
		void OpenAdditionalPanel(const PanelFactory& _factory);

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
		ViewportTarget*		m_pViewport		= nullptr;
		SceneManager*		m_pScenes		= nullptr;
		RHI::Device*		m_pDevice		= nullptr;
		EditorUIRenderer*	m_pUIRenderer	= nullptr;

		bool m_Initialized = false;

		// 既定レイアウトを組んだか。組み直すとユーザーの配置を壊すため一度きりにする
		bool m_LayoutInitialized = false;

		// 複数開けるパネルの作り方
		std::vector<PanelFactory> m_PanelFactories;
	};
}
