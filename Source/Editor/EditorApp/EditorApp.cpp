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

namespace
{
	// -------------------------------------------------------------------------------
	// メニューバー
	//
	// 画面上端に貼り付いた、動かせない細いウィンドウとして実装する
	// 高さは「ボタンの高さ + 上下の余白」から決める
	// これらを個別に決め打ちすると、ボタンが帯からはみ出して
	// 下のドッキング領域と重なってしまう
	// -------------------------------------------------------------------------------
	constexpr std::string_view	kMenuBarWindow		= "##EditorMenuBar";

	constexpr float kMenuButtonHeight	= 20.0f;
	constexpr float kMenuBarPaddingX	= 6.0f;
	constexpr float kMenuBarPaddingY	= 4.0f;
	constexpr float kMenuBarHeight		= kMenuButtonHeight + kMenuBarPaddingY * 2.0f;

	// メニューバーから開くドロップダウン
	constexpr std::string_view kWindowMenu = "WindowMenu";
	constexpr std::string_view kToolsMenu  = "ToolsMenu";
}

// -------------------------------------------------------------------------------
// 初期化
// -------------------------------------------------------------------------------
bool Editor::EditorApp::Init(
	EditorUI::Context*				_pUI,
	EditorUI::Font*					_pFont,
	ViewportTarget*					_pViewport,
	SceneManager*					_pScenes,
	RHI::Device*					_pDevice,
	EditorUIRenderer*				_pUIRenderer,
	const std::filesystem::path&	_contentRoot)
{
	if (_pUI == nullptr || _pFont == nullptr)
	{
		ELOG("EditorApp::Init() invalid argument");
		return false;
	}

	m_pUI			= _pUI;
	m_pFont			= _pFont;
	m_pViewport		= _pViewport;
	m_pScenes		= _pScenes;
	m_pDevice		= _pDevice;
	m_pUIRenderer	= _pUIRenderer;

	// コンテンツフォルダが無ければここで作られる
	if (!m_Assets.Init(_contentRoot))
	{
		ELOG("EditorApp::Init() AssetDatabase::Init failed");
		return false;
	}

	// -------------------------------------------------------------------------------
	// メニューバーの高さぶんは、ドッキング領域から外しておく
	//
	// 一度指定すれば以降は Context 側が画面サイズの変化に追従してくれる
	// 毎フレーム指定していたころは、ホバー判定が「メニューバーを含んだドック領域」で
	// 行われる瞬間があり、ドッキング中にメニューが押せなくなっていた
	// -------------------------------------------------------------------------------
	m_pUI->SetDockAreaInsetTop(kMenuBarHeight);

	RegisterPanelFactories();
	RegisterDefaultPanels();

	m_Initialized = true;
	return true;
}

void Editor::EditorApp::Term()
{
	m_pUI			= nullptr;
	m_pFont			= nullptr;
	m_pViewport		= nullptr;
	m_pScenes		= nullptr;
	m_pDevice		= nullptr;
	m_pUIRenderer	= nullptr;

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
	if (m_Panels.CountOfType(_factory.TypeName) >= _factory.MaxInstances)
	{
		return;	// 上限に達している
	}

	for (int index = 1; index <= _factory.MaxInstances; ++index)
	{
		// 作り方と同じ規則でタイトルを組み立て、空いている番号を探す
		const std::string title = (index <= 1)
			? _factory.TypeName
			: _factory.TypeName + " " + std::to_string(index);

		if (m_Panels.Find(title) != nullptr)
		{
			continue;	// その番号は使用中
		}

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

	// パネルは「表示されていない = 描く必要がない」を毎フレーム申告し直す
	if (m_pViewportPanel != nullptr)
	{
		m_pViewportPanel->BeginFrame();
	}

	BuildContext(_deltaTime);
	ValidateSelection();

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

	const int leftLeaf   = ui.SplitDockArea(EditorUI::DockSplitDir::Left,   0.22f);
	ui.DockWindow("Hierarchy", leftLeaf);

	const int rightLeaf  = ui.SplitDockArea(EditorUI::DockSplitDir::Right,  0.26f);
	ui.DockWindow("Inspector", rightLeaf);

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
	m_Context.pUI			= m_pUI;
	m_Context.pFont			= m_pFont;
	m_Context.pPanels		= &m_Panels;
	m_Context.pSelection	= &m_Selection;
	m_Context.pAssets		= &m_Assets;
	m_Context.pScenes		= m_pScenes;
	m_Context.pViewport		= m_pViewport;
	m_Context.pDevice		= m_pDevice;
	m_Context.pUIRenderer	= m_pUIRenderer;
	m_Context.DeltaTime		= _deltaTime;

	m_Context.pObjects = nullptr;

	if (m_pScenes != nullptr)
	{
		if (IScene* pScene = m_pScenes->GetCurrentScene())
		{
			m_Context.pObjects = pScene->GetObjectManager();
		}
	}
}

// -------------------------------------------------------------------------------
// 選択中オブジェクトの生存確認
//
// Selectionは生ポインタを持つため、シーンから消えた場合に備えて毎フレーム確認する
// -------------------------------------------------------------------------------
void Editor::EditorApp::ValidateSelection()
{
	GameObjectManager* pObjects = m_Context.pObjects;

	m_Selection.Validate(
		[pObjects](const GameObject* _pObject)
		{
			if (pObjects == nullptr)
			{ return false; }

			const auto& objects = pObjects->GetObjects();

			return std::any_of(objects.begin(), objects.end(),
				[_pObject](const std::unique_ptr<GameObject>& _candidate)
				{
					return _candidate.get() == _pObject;
				});
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

	// 毎フレーム強制的に画面上端へ合わせる。ウィンドウサイズの変更にも追従する
	ui.SetNextWindowPlacementForced(
		{ screen.Min.x, screen.Min.y },
		{ screen.Width(), kMenuBarHeight });

	// 帯の高さぴったりに収まるよう、このウィンドウだけ余白を小さくする
	ui.SetNextWindowPadding({ kMenuBarPaddingX, kMenuBarPaddingY });

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

		if (EditorUI::Button(ui, "ウィンドウ", *m_pFont, { 90.0f, kMenuButtonHeight }))
		{
			openMenuUnderLastItem(kWindowMenu);
		}

		EditorUI::SameLine(ui);

		if (EditorUI::Button(ui, "ツール", *m_pFont, { 72.0f, kMenuButtonHeight }))
		{
			openMenuUnderLastItem(kToolsMenu);
		}

		// -------------------------------------------------------------------------------
		// ウィンドウメニュー : 常設パネルの表示切り替え
		// パネルを増やしてもここは書き換え不要になるよう、一覧から自動で並べる
		// -------------------------------------------------------------------------------
		if (EditorUI::BeginPopup(ui, kWindowMenu))
		{
			// -------------------------------------------------------------------------------
			// いま存在するパネルの表示切り替え
			// -------------------------------------------------------------------------------
			for (const auto& panel : m_Panels.GetPanels())
			{
				if (panel == nullptr || panel->IsTransient())
				{
					continue;	// 一時パネルはメニューに載せない
				}

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
			// 「Inspector を2つ並べて別々のオブジェクトを見る」といった使い方のため
			// 上限に達した項目は、押せないことが分かるよう灰色で表示する
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
		// ツールメニュー : 動的に開くウィンドウ
		// -------------------------------------------------------------------------------
		if (EditorUI::BeginPopup(ui, kToolsMenu))
		{
			if (EditorUI::MenuItem(ui, *m_pFont, "新規エフェクトエディタ"))
			{
				EffectEditorPanel::OpenScratch(m_Context);
			}

			EditorUI::MenuSeparator(ui);

			if (EditorUI::MenuItem(ui, *m_pFont, "コンテンツを再読み込み"))
			{
				m_Assets.Refresh();
			}

			EditorUI::EndPopup(ui);
		}
	}
	ui.EndWindow();
}

// -------------------------------------------------------------------------------
// 自前のGPU描画を持つパネルの描画
// -------------------------------------------------------------------------------
void Editor::EditorApp::RenderPanels(ID3D12GraphicsCommandList* _pCmd)
{
	if (!m_Initialized)
	{ return; }

	m_Panels.RenderAll(m_Context, _pCmd);
}

// -------------------------------------------------------------------------------
// ゲーム画面として必要な大きさ
// -------------------------------------------------------------------------------
bool Editor::EditorApp::GetRequestedViewportSize(uint32_t& _outWidth, uint32_t& _outHeight) const
{
	if (m_pViewportPanel == nullptr || !m_pViewportPanel->IsVisibleThisFrame())
	{
		return false;
	}

	_outWidth	= m_pViewportPanel->GetRequestedWidth();
	_outHeight	= m_pViewportPanel->GetRequestedHeight();

	return _outWidth > 0 && _outHeight > 0;
}

bool Editor::EditorApp::IsViewportHovered() const
{
	return m_pViewportPanel != nullptr && m_pViewportPanel->IsViewportHovered();
}
