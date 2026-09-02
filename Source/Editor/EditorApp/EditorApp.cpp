// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "EditorApp.h"

#include <Engine/EditorUI/Widgets/Widgets.h>
#include <Engine/EditorUI/Widgets/Layout/Layout.h>
#include <Engine/Scene/SceneManager/SceneManager.h>
#include <Engine/GameObject/GameObjectManager.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

#include <Editor/Panels/ViewportPanel/ViewportPanel.h>
#include <Editor/Panels/HierarchyPanel/HierarchyPanel.h>
#include <Editor/Panels/InspectorPanel/InspectorPanel.h>
#include <Editor/Panels/ContentBrowserPanel/ContentBrowserPanel.h>
#include <Editor/Panels/EffectEditorPanel/EffectEditorPanel.h>
#include <Editor/Commands/ObjectCommands/ObjectCommands.h>
#include <Editor/Prefab/PrefabSystem/PrefabSystem.h>
#include <Editor/Scene/SceneEditor/SceneEditor.h>
#include <Engine/Asset/PrefabAsset/PrefabAsset.h>
#include <Engine/Asset/SceneAsset/SceneAsset.h>

namespace
{
	// -------------------------------------------------------------------------------
	// メニューバー
	//
	// メニューバーは通常のDockWindowではなく、
	// Editor画面の最上部に常時固定される専用Windowとして扱う
	// 
	// DockSpaceとは領域を分離しておくことで、Dockレイアウトを変更しても
	// メニューバーの位置やサイズが影響を受けないようにしておく
	// -------------------------------------------------------------------------------

	// "##"から始まる名前はユーザーに表示するタイトルではなく、
	// EditorUI内部でWindowを一意に識別するためのIDとして使用する
	constexpr std::string_view	kMenuBarWindow		= "##EditorMenuBar";

	// メニューバーの高さは固定値を直接参照するのではなく、
	// ボタン本体＋上下余白 から算出する
	// ボタン側のサイズを変更した際にもメニューバー全体の高さが追従し、
	// ボタンがDock領域へはみ出すことを防ぐため。
	constexpr float kMenuButtonHeight	= 20.0f;
	constexpr float kMenuBarPaddingX	= 6.0f;
	constexpr float kMenuBarPaddingY	= 4.0f;
	constexpr float kMenuBarHeight		= kMenuButtonHeight + kMenuBarPaddingY * 2.0f;

	// メニューバーから開くドロップダウン
	constexpr std::string_view kCreateMenu = "CreateMenu";
	constexpr std::string_view kEditMenu   = "EditMenu";
	constexpr std::string_view kSceneMenu  = "SceneMenu";
	constexpr std::string_view kWindowMenu = "WindowMenu";
	constexpr std::string_view kToolsMenu  = "ToolsMenu";
}

// -------------------------------------------------------------------------------
// 初期化
// 
//	EditorAppが利用する各Engineシステムへの参照を受け取り、
//	AssetDatabase,panel,DockSpaceなどのEditorUI全体の初期状態を構築する
// -------------------------------------------------------------------------------
bool Editor::EditorApp::Init(
	EditorUI::Context*				_pUI,
	EditorUI::Font*					_pFont,
	ViewportTarget*					_pViewport,
	SceneManager*					_pScenes,
	RHI::Device*					_pDevice,
	EditorUIRenderer*				_pUIRenderer,
	ModelLibrary*					_pModels,
	const std::filesystem::path&	_contentRoot)
{
	if (_pUI == nullptr || _pFont == nullptr)
	{
		ELOG("EditorApp::Init() invalid argument");
		return false;
	}

	// -------------------------------------------------------------------------------
	// Engineシステムへの参照を保持
	// -------------------------------------------------------------------------------
	m_pUI			= _pUI;
	m_pFont			= _pFont;
	m_pViewport		= _pViewport;
	m_pScenes		= _pScenes;
	m_pDevice		= _pDevice;
	m_pUIRenderer	= _pUIRenderer;
	m_pModels		= _pModels;

	// -------------------------------------------------------------------------------
	// AssetDatabaseの初期化
	// 
	//	ContentBrowserが参照するコンテンツルートを設定する
	//	指定されたディレクトリが存在しない場合はAssetDatabase側で生成される
	// -------------------------------------------------------------------------------
	if (!m_Assets.Init(_contentRoot))
	{
		ELOG("EditorApp::Init() AssetDatabase::Init failed");
		return false;
	}

	// -------------------------------------------------------------------------------
	// DockSpaceからメニューバー領域を削除
	//
	// 最上部のkMenuBarHeight分をDock対象外として確保する
	// Context側が画面サイズ変更へ追従するため、この設定は初期化時に一度だけ行う
	// -------------------------------------------------------------------------------
	m_pUI->SetDockAreaInsetTop(kMenuBarHeight);

	// 複数生成可能なPanelの生成方法を先に登録し、
	// その後で起動時に常設するPanelインスタンスを生成する
	RegisterPanelFactories();
	RegisterDefaultPanels();

	// すべての初期化が成功してからtrueにする
	m_Initialized = true;
	return true;
}

void Editor::EditorApp::Term()
{
	// -------------------------------------------------------------------------------
	// 履歴を先に捨てる
	//
	// 削除されたオブジェクトの実体を抱えているため、
	// エンジン側の終了処理が進む前に手放しておく
	// -------------------------------------------------------------------------------
	m_History.Clear();
	m_pPreviousObjects = nullptr;

	m_pUI			= nullptr;
	m_pFont			= nullptr;
	m_pViewport		= nullptr;
	m_pScenes		= nullptr;
	m_pDevice		= nullptr;
	m_pUIRenderer	= nullptr;
	m_pModels		= nullptr;

	m_Initialized = false;
}

// -------------------------------------------------------------------------------
// 常設パネルの登録
//
// 初期配置はUE5に近い並びにしておく
//	左		ヒエラルキー
//	中央	ビューポート
//	右		インスペクタ
//	下		コンテンツブラウザ
//
// ドッキングは利用者が自由に組み替えられるため、ここで決めるのは
// 「まだ何もドッキングしていない状態での見え方」だけ
// -------------------------------------------------------------------------------
void Editor::EditorApp::RegisterDefaultPanels()
{
	auto viewport = std::make_unique<ViewportPanel>();
	m_pViewportPanel = viewport.get();

	m_Panels.Add(std::move(viewport));
	m_Panels.Add(std::make_unique<HierarchyPanel>());
	m_Panels.Add(std::make_unique<InspectorPanel>());
	m_Panels.Add(std::make_unique<ContentBrowserPanel>());
}

// -------------------------------------------------------------------------------
// 複数開けるパネルの作り方を登録する
//
// ビューポートをここへ入れていないのは、1枚につき専用のレンダーターゲットと
// シーンの再描画が必要になり、他のパネルと同じ扱いにできないため
// -------------------------------------------------------------------------------
void Editor::EditorApp::RegisterPanelFactories()
{
	m_PanelFactories.push_back({
		"Hierarchy", "ヒエラルキーを追加",
		[](int _index) { return std::make_unique<HierarchyPanel>(_index); }, 4 });

	m_PanelFactories.push_back({
		"Inspector", "インスペクタを追加",
		[](int _index) { return std::make_unique<InspectorPanel>(_index); }, 4 });

	m_PanelFactories.push_back({
		"Content Browser", "コンテンツブラウザを追加",
		[](int _index) { return std::make_unique<ContentBrowserPanel>(_index); }, 4 });
}

// -------------------------------------------------------------------------------
// 同じ種類のパネルをもう1枚開く
//
// 通し番号は「まだ使われていない最小の番号」を選ぶ
// 途中の1枚を閉じたあとでも、番号が飛ばずに埋まっていく
// -------------------------------------------------------------------------------
void Editor::EditorApp::OpenAdditionalPanel(const PanelFactory& _factory)
{
	// 種類ごとに設定された最大インスタンス数を超えて生成しない
	if (m_Panels.CountOfType(_factory.TypeName) >= _factory.MaxInstances)
	{
		return;	// 上限に達している
	}

	// -------------------------------------------------------------------------------
	// 使用されていない最小のインスタンス番号を探す
	// -------------------------------------------------------------------------------
	for (int index = 1; index <= _factory.MaxInstances; ++index)
	{
		// panel生成側と同じ命名規則でタイトルを構築する
		const std::string title = (index <= 1)
			? _factory.TypeName
			: _factory.TypeName + " " + std::to_string(index);

		// 同じタイトルが存在する場合、その場合は既に使用されている
		if (m_Panels.Find(title) != nullptr)
		{
			continue;	// その番号は使用中
		}

		// 最初に見つかった空き番号でPanelを生成する
		m_Panels.Add(_factory.Create(index));
		return;
	}
}

// -------------------------------------------------------------------------------
// 1フレーム分のUIを組み立てる
// -------------------------------------------------------------------------------
void Editor::EditorApp::BuildUI(float _deltaTime)
{
	if (!m_Initialized)
	{ return; }

	// ViewportPanelのフレーム状態を初期化
	if (m_pViewportPanel != nullptr)
	{
		m_pViewportPanel->BeginFrame();
	}

	// 各Panelが利用する共通Contextを最新情報へ更新する
	BuildContext(_deltaTime);
	// Scene変更やGameObject削除によって、Selectionが無効なオブジェクトを指していないか確認
	ValidateSelection();

	// -------------------------------------------------------------------------------
	// キーボードショートカット
	//
	// パネルを描く前に処理する
	// 先に済ませておくことで、Undoの結果がこのフレームの表示へそのまま反映される
	// -------------------------------------------------------------------------------
	ProcessShortcuts();

	// 画面サイズが確定したこの時点で、初回だけ既定レイアウトを組む
	if (!m_LayoutInitialized)
	{
		BuildDefaultLayout();
		m_LayoutInitialized = true;
	}

	DrawMenuBar();

	m_Panels.DrawAll(m_Context);
}

// -------------------------------------------------------------------------------
// 既定のドッキングレイアウト
//
//	+----------------------------------------------+
//	| メニューバー                                 |
//	+-----------+------------------------+---------+
//	| Hierarchy |       Viewport         |Inspector|
//	|           +------------------------+         |
//	|           |    Content Browser     |         |
//	+-----------+------------------------+---------+
//
// UE5の既定配置に合わせている
// 一度組んだあとはユーザーの操作が優先されるため、ここは初回だけ通る
// -------------------------------------------------------------------------------
void Editor::EditorApp::BuildDefaultLayout()
{
	EditorUI::Context& ui = *m_pUI;

	// すでに何かがドッキングされている場合は触らない
	if (!ui.IsDockSpaceEmpty())
	{ return; }

	// -------------------------------------------------------------------------------
	// 手順
	//	1. まだ空のルート区画へビューポートを置き、これを「中央」とする
	//	2. 外周を1辺ずつ切り出して、そこへ他のパネルを置く
	//
	//	外周を先に切り出すと、あとから「残った中央」を座標で探し当てる必要があり、
	//	分割の途中経過に依存して壊れやすくなる
	//	中央を先に確定させることで、どの区画がどれかが常に明確になる
	// -------------------------------------------------------------------------------
	ui.DockWindow("Viewport", ui.GetRootDockLeafId());

	// DockSpace左側22%をHierarchy用として切り出す
	const int leftLeaf   = ui.SplitDockArea(EditorUI::DockSplitDir::Left,   0.22f);
	ui.DockWindow("Hierarchy", leftLeaf);

	// 残った領域の右側26%をInspector用として切り出す
	const int rightLeaf  = ui.SplitDockArea(EditorUI::DockSplitDir::Right,  0.26f);
	ui.DockWindow("Inspector", rightLeaf);

	// 残った中央領域の下側30%をContentBrowser用として切り出す
	const int bottomLeaf = ui.SplitDockArea(EditorUI::DockSplitDir::Bottom, 0.30f);
	ui.DockWindow("Content Browser", bottomLeaf);

}

// -------------------------------------------------------------------------------
// 毎フレームのEditorContextを組み立てる
//
// シーンは切り替わることがあるため、GameObjectManagerは毎フレーム引き直す
// -------------------------------------------------------------------------------
void Editor::EditorApp::BuildContext(float _deltaTime)
{
	// -------------------------------------------------------------------------------
	// Editor全体で共通する参照
	// -------------------------------------------------------------------------------
	m_Context.pUI			= m_pUI;
	m_Context.pFont			= m_pFont;
	m_Context.pPanels		= &m_Panels;
	m_Context.pSelection	= &m_Selection;
	m_Context.pAssets		= &m_Assets;
	m_Context.pHistory		= &m_History;
	m_Context.pScenes		= m_pScenes;
	m_Context.pViewport		= m_pViewport;
	m_Context.pDevice		= m_pDevice;
	m_Context.pUIRenderer	= m_pUIRenderer;
	m_Context.pModels		= m_pModels;

	// Panel側でアニメーションや時間依存処理を行えるよう、現在フレームの経過時間もContext経由で共有
	m_Context.DeltaTime		= _deltaTime;

	// 現在SceneのGameObjectManagerを取得
	// Scene切り替え直後などでも前SceneのManagerを参照しないよう、まずnullptrで初期化
	m_Context.pObjects = nullptr;

	if (m_pScenes != nullptr)
	{
		if (IScene* pScene = m_pScenes->GetCurrentScene())
		{
			m_Context.pObjects = pScene->GetObjectManager();
		}
	}

	// -------------------------------------------------------------------------------
	// シーンが切り替わったら履歴を捨てる
	//
	// 履歴は前のシーンのGameObjectを指し、削除されたものは実体まで抱えている
	// そのまま残すと、別のシーンへ前のシーンのオブジェクトを復元してしまう
	// -------------------------------------------------------------------------------
	if (m_Context.pObjects != m_pPreviousObjects)
	{
		m_History.Clear();
		m_pPreviousObjects = m_Context.pObjects;
	}
}

// -------------------------------------------------------------------------------
// 選択中オブジェクトの生存確認
//
// Selectionは生ポインタを持つため、シーンから消えた場合に備えて毎フレーム確認する
// -------------------------------------------------------------------------------
void Editor::EditorApp::ValidateSelection()
{
	// -------------------------------------------------------------------------------
	// 現在SceneのGameObjectManagerを取得
	// -------------------------------------------------------------------------------
	
	// BuildContextによって現在のSceneのObjectManagerが設定されている。
	// Sceneが存在しない場合や切り替え途中の場合はnullptrになる可能性があるため、
	// この値自体もValidation条件に含める
	GameObjectManager* pObjects = m_Context.pObjects;

	// -------------------------------------------------------------------------------
	// Selectionへ生存判定関数を渡す
	// -------------------------------------------------------------------------------

	m_Selection.Validate(
		[pObjects](const GameObject* _pObject)
		{
			// Sceneが存在しない場合、そのScene由来の選択状態も無効とする
			if (pObjects == nullptr)
			{ return false; }

			// pObjectsの中に_pObjectが存在するかどうか返す
			return pObjects->Contains(_pObject);
		});
}

// -------------------------------------------------------------------------------
// メニューバー
//
// 画面上端に固定した、タイトルバーもリサイズもないウィンドウとして描く
// ドッキング対象から外しているため、レイアウトを組み替えても常に上端に残る
// -------------------------------------------------------------------------------
void Editor::EditorApp::DrawMenuBar()
{
	EditorUI::Context& ui = *m_pUI;

	const EditorUI::Rect2D& screen = ui.GetScreenBounds();

	// -------------------------------------------------------------------------------
	// 画面上端へ毎フレーム配置
	// 
	// Windowサイズ変更時にも画面幅へ追従させるため、
	// Position / Sizeは固定保存せず、現在のScreenBoundsから毎フレーム算出する
	// -------------------------------------------------------------------------------
	ui.SetNextWindowPlacementForced(
		{ screen.Min.x, screen.Min.y },
		{ screen.Width(), kMenuBarHeight });

	// 通常のWindowのPaddingでは帯が必要以上に高くなるため、
	// MenuBar専用の小さいPaddingを設定する
	ui.SetNextWindowPadding({ kMenuBarPaddingX, kMenuBarPaddingY });

	// -------------------------------------------------------------------------------
	// MenuBarに不要な通常Window機能を無効化
	// -------------------------------------------------------------------------------
	const EditorUI::WindowFlags flags =
		EditorUI::WindowFlags::NoTitleBar |
		EditorUI::WindowFlags::NoResize |
		EditorUI::WindowFlags::NoMove |
		EditorUI::WindowFlags::NoScrollbar |
		EditorUI::WindowFlags::NoDock;

	if (ui.BeginWindow(kMenuBarWindow, nullptr, flags))
	{
		// -------------------------------------------------------------------------------
		// 見出しボタンを押すと、その真下にドロップダウンが開く
		// マウス位置ではなくボタンの下端に合わせることで、
		// 押した場所とメニューの位置が対応して分かりやすくなる
		// -------------------------------------------------------------------------------
		const auto openMenuUnderLastItem = [&ui](std::string_view _menuName)
		{
			if (const EditorUI::WindowFrame* pFrame = ui.GetCurrentWindow())
			{
				ui.OpenPopupAt(_menuName, { pFrame->LastItemRect.Min.x, pFrame->LastItemRect.Max.y });
			}
		};

		// -------------------------------------------------------------------------------
		// 作成メニュー
		//
		// シーンへ新しいGameObjectを追加する入り口
		// いちばん使う操作なので先頭に置く
		// -------------------------------------------------------------------------------
		if (EditorUI::Button(ui, "作成", *m_pFont, { 64.0f, kMenuButtonHeight }))
		{
			openMenuUnderLastItem(kCreateMenu);
		}

		EditorUI::SameLine(ui);

		// -------------------------------------------------------------------------------
		// 編集メニュー
		//
		// 元に戻す / やり直す の入り口
		// ショートカットからも同じ処理を呼ぶが、
		// 「何ができるか」が目に見える場所にも必要なため項目として置く
		// -------------------------------------------------------------------------------
		if (EditorUI::Button(ui, "編集", *m_pFont, { 64.0f, kMenuButtonHeight }))
		{
			openMenuUnderLastItem(kEditMenu);
		}

		EditorUI::SameLine(ui);

		// -------------------------------------------------------------------------------
		// シーンメニュー
		//
		// 今の並びを丸ごと保存したり、別のシーンを開いたりする入り口
		// -------------------------------------------------------------------------------
		if (EditorUI::Button(ui, "シーン", *m_pFont, { 72.0f, kMenuButtonHeight }))
		{
			openMenuUnderLastItem(kSceneMenu);
		}

		EditorUI::SameLine(ui);

		// -------------------------------------------------------------------------------
		// Windowメニュー
		// -------------------------------------------------------------------------------
		if (EditorUI::Button(ui, "ウィンドウ", *m_pFont, { 90.0f, kMenuButtonHeight }))
		{
			openMenuUnderLastItem(kWindowMenu);
		}

		EditorUI::SameLine(ui);

		// -------------------------------------------------------------------------------
		// Toolsメニュー
		// -------------------------------------------------------------------------------
		if (EditorUI::Button(ui, "ツール", *m_pFont, { 72.0f, kMenuButtonHeight }))
		{
			openMenuUnderLastItem(kToolsMenu);
		}

		// -------------------------------------------------------------------------------
		// CreatePopup
		//
		// シーンへオブジェクトを追加・複製・削除する
		// -------------------------------------------------------------------------------
		if (EditorUI::BeginPopup(ui, kCreateMenu))
		{
			DrawCreateMenu();
			EditorUI::EndPopup(ui);
		}

		// -------------------------------------------------------------------------------
		// EditPopup
		//
		// 操作の履歴をたどる
		// -------------------------------------------------------------------------------
		if (EditorUI::BeginPopup(ui, kEditMenu))
		{
			DrawEditMenu();
			EditorUI::EndPopup(ui);
		}

		// -------------------------------------------------------------------------------
		// ScenePopup
		//
		// シーンの新規作成・保存・読み込み
		// -------------------------------------------------------------------------------
		if (EditorUI::BeginPopup(ui, kSceneMenu))
		{
			DrawSceneMenu();
			EditorUI::EndPopup(ui);
		}

		// -------------------------------------------------------------------------------
		// WindowPopup
		// 
		// 現在表示するPanelの表示切替と、複数生成可能なPanelの追加を行う
		// -------------------------------------------------------------------------------
		if (EditorUI::BeginPopup(ui, kWindowMenu))
		{
			// -------------------------------------------------------------------------------
			// 現在存在するパネルの表示切り替え
			// 
			// Panel一覧から自動生成することで、
			// Panelの追加のたびにMenuBarコードを変更する必要をなくす。
			// -------------------------------------------------------------------------------
			for (const auto& panel : m_Panels.GetPanels())
			{
				if (panel == nullptr || panel->IsTransient())
				{
					continue;	// 一時パネルはメニューに載せない
				}

				// 現在表示中のPanelには"*"をつけ、
				// MenuItemを見るだけで表示状態を判別できるようにする
				const std::string label =
					(panel->IsOpen() ? "* " : "  ") + panel->GetTitle();

				if (EditorUI::MenuItem(ui, *m_pFont, label))
				{
					panel->SetOpen(!panel->IsOpen());
				}
			}

			EditorUI::MenuSeparator(ui);

			// -------------------------------------------------------------------------------
			// 同じ種類をもう1枚開く
			//
			// Inspectorを複数並べるなど、同種類のPanelを複数使用するための項目
			// 現在数 / 最大数も表示し、上限到達時には選択不可にする
			// -------------------------------------------------------------------------------
			for (const PanelFactory& factory : m_PanelFactories)
			{
				const int count = m_Panels.CountOfType(factory.TypeName);
				const bool canOpen = (count < factory.MaxInstances);

				const std::string label =
					factory.MenuLabel + "  (" +
					std::to_string(count) + " / " + std::to_string(factory.MaxInstances) + ")";

				if (EditorUI::MenuItem(ui, *m_pFont, label, canOpen))
				{
					OpenAdditionalPanel(factory);
				}
			}

			EditorUI::EndPopup(ui);
		}

		// -------------------------------------------------------------------------------
		// Tools Popup
		// 
		// 常設Panelではなく、必要に応じて起動するEditorツールや
		// Editor全体に対する操作を配置する
		// -------------------------------------------------------------------------------
		if (EditorUI::BeginPopup(ui, kToolsMenu))
		{
			// 新しい一時EffectEditorを開く
			if (EditorUI::MenuItem(ui, *m_pFont, "新規エフェクトエディタ"))
			{
				EffectEditorPanel::OpenScratch(m_Context);
			}

			EditorUI::MenuSeparator(ui);

			// FileWatcherの自動検知とは別に、ユーザーが明示的にContent一覧を再走査できるようにする
			if (EditorUI::MenuItem(ui, *m_pFont, "コンテンツを再読み込み"))
			{
				// 現在表示しているフォルダの情報を再取得
				m_Assets.Refresh();
			}

			EditorUI::EndPopup(ui);
		}
	}

	// BeginWindow()がfalseの場合でもWindowの終了処理は必要なため、
	// BeginWindowのifブロック外で必ず呼び出す
	ui.EndWindow();
}

// -------------------------------------------------------------------------------
// 自前のGPU描画を持つパネルの描画
// 
// BuildUI()はEditorUIのWidget構築を担当するのに対し、
// この関数ではViewportなど独自のDirectX12描画を持つPanelを処理する
// -------------------------------------------------------------------------------
void Editor::EditorApp::RenderPanels(ID3D12GraphicsCommandList* _pCmd)
{
	if (!m_Initialized)
	{ return; }

	m_Panels.RenderAll(m_Context, _pCmd);
}

// -------------------------------------------------------------------------------
// 「作成」メニューの中身
//
// すべて基底クラスの GameObject をそのまま使う
// 見た目や機能は、あとから Component を足して育てていく想定
//
// 実際の生成処理は GameObjectFactory が持つ
// ヒエラルキーの右クリックからも同じ関数を呼ぶため、挙動が食い違わない
// -------------------------------------------------------------------------------
void Editor::EditorApp::DrawCreateMenu()
{
	EditorUI::Context& ui = *m_pUI;

	// シーンが無いときは、押しても何も起きないことを見た目で示す
	const bool hasScene = (m_Context.pObjects != nullptr);

	// -------------------------------------------------------------------------------
	// 新規作成
	//
	// 名前を分けているのは、ヒエラルキー上で役割を見分けやすくするため
	// 中身はどれも「Transformだけを持つGameObject」で同じ
	// -------------------------------------------------------------------------------
	if (EditorUI::MenuItem(ui, *m_pFont, "空のオブジェクト", hasScene))
	{
		ObjectCommands::CreateEmpty(m_Context, "New Object");
	}

	if (EditorUI::MenuItem(ui, *m_pFont, "グループ", hasScene))
	{
		// 子をまとめる目印として置くオブジェクト
		ObjectCommands::CreateEmpty(m_Context, "Group");
	}

	if (EditorUI::MenuItem(ui, *m_pFont, "スポーン地点", hasScene))
	{
		// 出現位置の目印。位置だけを持つ用途
		ObjectCommands::CreateEmpty(m_Context, "Spawn Point");
	}

	EditorUI::MenuSeparator(ui);

	// -------------------------------------------------------------------------------
	// 選択中のオブジェクトに対する操作
	// -------------------------------------------------------------------------------
	GameObject* pSelected = m_Selection.GetObject();
	const bool hasSelection = (pSelected != nullptr);

	if (EditorUI::MenuItem(ui, *m_pFont, "選択オブジェクトを複製", hasSelection))
	{
		ObjectCommands::Duplicate(m_Context, pSelected);
	}

	if (EditorUI::MenuItem(ui, *m_pFont, "選択オブジェクトを削除", hasSelection))
	{
		ObjectCommands::Destroy(m_Context, pSelected);
	}

	EditorUI::MenuSeparator(ui);

	// -------------------------------------------------------------------------------
	// プレファブ（オブジェクトの設計図）
	//
	//	保存 : 今の構成をコンテンツフォルダへ書き出す
	//	配置 : コンテンツブラウザで選んでいるプレファブをシーンへ置く
	//
	//	どちらもコンテンツブラウザ側からも行えるが、
	//	「作成」の流れの中にも置いておくと、
	//	作る → 整える → 設計図にする がひと続きになる
	// -------------------------------------------------------------------------------
	if (EditorUI::MenuItem(ui, *m_pFont, "選択オブジェクトをプレファブとして保存", hasSelection))
	{
		std::filesystem::path savedPath;

		if (PrefabSystem::SaveToContent(m_Context, *pSelected, savedPath))
		{
			// 保存したファイルをそのまま選択し、どこに出来たかを分かるようにする
			m_Selection.SelectAsset(savedPath);
		}
	}

	// コンテンツブラウザでプレファブを選んでいるときだけ有効
	const bool prefabSelected =
		m_Selection.GetType() == SelectionType::Asset &&
		PrefabIO::IsPrefabPath(m_Selection.GetAssetPath());

	if (EditorUI::MenuItem(ui, *m_pFont, "選択プレファブをシーンへ配置", prefabSelected && hasScene))
	{
		PrefabSystem::InstantiateFromFile(m_Context, m_Selection.GetAssetPath());
	}
}

// -------------------------------------------------------------------------------
// 編集メニュー
// -------------------------------------------------------------------------------
void Editor::EditorApp::DrawEditMenu()
{
	EditorUI::Context& ui = *m_pUI;

	// -------------------------------------------------------------------------------
	// 戻る先 / 進む先の操作名を項目に含める
	//
	// 「元に戻す」だけでは何が戻るのか分からないため、
	// 「元に戻す : オブジェクトの削除」のように直前の操作を添える
	// -------------------------------------------------------------------------------
	const auto makeLabel = [](std::string_view _action, std::string_view _target)
	{
		std::string label(_action);

		if (!_target.empty())
		{
			label += " : ";
			label += _target;
		}

		return label;
	};

	if (EditorUI::MenuItem(ui, *m_pFont,
		makeLabel("元に戻す", m_History.GetUndoLabel()), m_History.CanUndo()))
	{
		m_History.Undo(m_Context);
	}

	if (EditorUI::MenuItem(ui, *m_pFont,
		makeLabel("やり直す", m_History.GetRedoLabel()), m_History.CanRedo()))
	{
		m_History.Redo(m_Context);
	}

	EditorUI::MenuSeparator(ui);

	// -------------------------------------------------------------------------------
	// どこまで戻れるかの目安
	//
	// 選べない項目として置き、表示専用であることを見た目で示す
	// -------------------------------------------------------------------------------
	EditorUI::MenuItem(ui, *m_pFont,
		"戻せる操作 : " + std::to_string(m_History.GetUndoCount()) + " 件" +
		"   やり直せる操作 : " + std::to_string(m_History.GetRedoCount()) + " 件",
		false);

	EditorUI::MenuSeparator(ui);

	GameObject* pSelected	= m_Selection.GetObject();
	const bool hasSelection	= (pSelected != nullptr);

	if (EditorUI::MenuItem(ui, *m_pFont, "複製  (Ctrl+D)", hasSelection))
	{
		ObjectCommands::Duplicate(m_Context, pSelected);
	}

	if (EditorUI::MenuItem(ui, *m_pFont, "削除  (Delete)", hasSelection))
	{
		ObjectCommands::Destroy(m_Context, pSelected);
	}
}

// -------------------------------------------------------------------------------
// シーンメニュー
//
// シーンを開くと今の中身がすべて入れ替わるため、
// 「開く」より先に「保存」を並べて、保存し忘れにくい順にしている
// -------------------------------------------------------------------------------
void Editor::EditorApp::DrawSceneMenu()
{
	EditorUI::Context& ui = *m_pUI;

	const bool hasScene = (m_Context.pObjects != nullptr);

	// いま開いているシーン名。まだ保存していない場合は仮の名前が出る
	EditorUI::MenuItem(ui, *m_pFont, "現在 : " + SceneEditor::GetCurrentName(), false);

	EditorUI::MenuSeparator(ui);

	// -------------------------------------------------------------------------------
	// 保存
	//
	// 保存先はコンテンツブラウザで今開いているフォルダ
	// 作られたファイルをそのまま選択し、どこへ出来たかを分かるようにする
	// -------------------------------------------------------------------------------
	if (EditorUI::MenuItem(ui, *m_pFont, "上書き保存", hasScene && SceneEditor::HasCurrentPath()))
	{
		SceneEditor::Save(m_Context);
	}

	if (EditorUI::MenuItem(ui, *m_pFont, "名前を付けて保存", hasScene))
	{
		std::filesystem::path savedPath;

		if (SceneEditor::SaveAs(m_Context, SceneEditor::GetCurrentName(), savedPath))
		{
			m_Selection.SelectAsset(savedPath);
		}
	}

	EditorUI::MenuSeparator(ui);

	// -------------------------------------------------------------------------------
	// 開く / 新規作成
	//
	// どちらも今の中身が消える操作なので、区切り線で分けている
	// -------------------------------------------------------------------------------
	const bool sceneSelected =
		m_Selection.GetType() == SelectionType::Asset &&
		SceneIO::IsScenePath(m_Selection.GetAssetPath());

	if (EditorUI::MenuItem(ui, *m_pFont, "選択シーンを開く", sceneSelected && hasScene))
	{
		SceneEditor::Open(m_Context, m_Selection.GetAssetPath());
	}

	if (EditorUI::MenuItem(ui, *m_pFont, "新規シーン（すべて削除）", hasScene))
	{
		SceneEditor::CreateNew(m_Context);
	}

	EditorUI::MenuSeparator(ui);

	EditorUI::MenuItem(ui, *m_pFont,
		"シーンを開くとUndo履歴は消えます", false);
}

// -------------------------------------------------------------------------------
// キーボードショートカット
//
//	Ctrl+Z			元に戻す
//	Ctrl+Y			やり直す
//	Ctrl+Shift+Z	やり直す（別の慣習に合わせた割り当て）
//	Ctrl+D			選択オブジェクトを複製
//	Delete			選択オブジェクトを削除
// -------------------------------------------------------------------------------
void Editor::EditorApp::ProcessShortcuts()
{
	EditorUI::Context& ui = *m_pUI;

	// -------------------------------------------------------------------------------
	// 文字を入力している最中は何も処理しない
	//
	// 名前の編集中にCtrl+Zを押したときに、
	// 入力欄ではなくシーンが巻き戻ってしまうのを防ぐ
	// -------------------------------------------------------------------------------
	if (ui.GetTextEditState().Widget != 0)
	{
		return;
	}

	const bool ctrl		= ui.IsKeyDown(EditorUI::Key::Ctrl);
	const bool shift	= ui.IsKeyDown(EditorUI::Key::Shift);

	if (ctrl)
	{
		// Ctrl+Shift+Z を先に見る
		// 先にCtrl+Zを判定すると、Shiftを押していても「元に戻す」になってしまう
		if (shift && ui.IsKeyPressed(EditorUI::Key::Z))
		{
			// 取り消した操作を戻す
			m_History.Redo(m_Context);
			return;
		}

		if (ui.IsKeyPressed(EditorUI::Key::Z))
		{
			// 直前の動作を取り消す
			m_History.Undo(m_Context);
			return;
		}

		if (ui.IsKeyPressed(EditorUI::Key::Y))
		{
			// 取り消した操作を戻す
			m_History.Redo(m_Context);
			return;
		}

		if (ui.IsKeyPressed(EditorUI::Key::D))
		{
			// 既存のオブジェクトを複製
			ObjectCommands::Duplicate(m_Context, m_Selection.GetObject());
			return;
		}
	}

	// -------------------------------------------------------------------------------
	// Delete
	//
	// 修飾キーを押していないときだけ反応させる
	// Ctrl+Delete などに別の意味を割り当てられるようにしておくため
	// -------------------------------------------------------------------------------
	if (!ctrl && !shift && ui.IsKeyPressed(EditorUI::Key::Delete))
	{
		// オブジェクトを削除
		ObjectCommands::Destroy(m_Context, m_Selection.GetObject());
	}
}

// -------------------------------------------------------------------------------
// Viewportが必要としているScene描画サイズを取得
// 
// EditorWindow全体のサイズではなく、実際にViewportPanel内部で
// ゲーム画面を表示する領域のサイズをRendererへ通知するために使用する
// 
// Viewportが閉じているDockTabの裏側にある等、
// 今フレーム実際に表示されていない場合は描画自体を省略できるようにfalseを返す
// -------------------------------------------------------------------------------
bool Editor::EditorApp::GetRequestedViewportSize(uint32_t& _outWidth, uint32_t& _outHeight) const
{
	// Viewportが存在しない、または今フレーム表示されていない場合はRenderTargetを更新する必要がない
	if (m_pViewportPanel == nullptr || !m_pViewportPanel->IsVisibleThisFrame())
	{
		return false;
	}

	// Panel内で実際に使用可能だったContent領域のサイズを取得
	_outWidth	= m_pViewportPanel->GetRequestedWidth();
	_outHeight	= m_pViewportPanel->GetRequestedHeight();

	// 0サイズのRenderTarget生成・Resize要求を防ぐ
	return _outWidth > 0 && _outHeight > 0;
}

// -------------------------------------------------------------------------------
// マウスがViewport描画領域上に存在するか確認
// 
// Editor上でカメラ操作・オブジェクト選択などのゲームViewport入力を
// 受け付けるかどうかを判断するために使用する
// Panel全体ではなく、Viewportの実描画領域に対するHover状態を返す
// -------------------------------------------------------------------------------
bool Editor::EditorApp::IsViewportHovered() const
{
	return m_pViewportPanel != nullptr && m_pViewportPanel->IsViewportHovered();
}
