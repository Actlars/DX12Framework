// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Context.h"
#include <Engine/EditorUI/Text/Font/Font.h>
#include <Engine/EditorUI/Text/TextLayout/TextLayout.h>
#include <Engine/Utility/StringUtil/StringUtil.h>

EditorUI::Context::Context()
{
	// コンストラクタでスタイルの初期値をコピー
	m_Style = GetDefaultStyle();
}

// -------------------------------------------------------------------------------
// フレーム開始
//
// 「入力を確定してから、前フレームの配置に対してホバー/フォーカス/ドッキングを決める」
// という順序をここだけで管理する
// -------------------------------------------------------------------------------
void EditorUI::Context::NewFrame(const EditorUI::InputState& _input)
{
	// 1. 入力を最初に更新し、このフレームのClicked/Releasedを確定する
	m_InputTracker.NewFrame(_input);

	// カーソルの希望は毎フレーム作り直す
	// 前フレームの形が残ると、ウィジェットから離れても戻らなくなる
	m_MouseCursor = EditorUI::MouseCursor::Arrow;

	// 2. WindowFrameはWindowStateへの生ポインタを持つ
	//	  永続Window削除よりも先にフレーム限定参照を破棄する
	m_IdStack.Clear();
	m_FrameContext.NewFrame();
	m_PopupDrawOrder.clear();
	m_PopupStack.clear();

	// 3. 安全なフレーム境界で遅延削除を全サブシステムへ同期する
	ProcessPendingDestroyWindows();

	// 4. Widget/Dockの前フレーム操作状態を更新
	m_WidgetInteractionState.NewFrame();
	m_DockController.NewFrame(m_InputTracker);

	// Splitterは通常WindowのFocus/TitleBarより先にPointerを確保する
	m_DockController.UpdateSplitters(
		m_InputTracker, m_Style,
		!m_WindowInteraction.IsBusy() && !m_WidgetInteractionState.IsAnyActive());

	// 5. Hover/Focusは前フレームに確定したWindow配置で判定する
	//	  今フレームのBeginWindowを持つと入力が1フレーム遅れるため
	m_WindowManager.SetHovered(FindTopmostWindowAt(m_InputTracker.GetMousePos()));

	// 6. ポップアップの範囲外クリックは、フォーカス移動より先に処理する
	//	  背後のウィンドウを触らずにメニューだけ閉じられるようにするため
	UpdatePopupDismiss();

	HandleWindowFocusClick();

	// 7. FloatingMoveのRelease時はCaptureを解放する前にDock判定する
	if (m_InputTracker.IsMouseReleased(EditorUI::MouseButton::Mouse_Left))
	{
		const EditorUI::Id movingWindow = m_WindowInteraction.GetMovingWindow();
		if (movingWindow != 0 && m_WindowManager.Find(movingWindow) != nullptr)
		{
			m_DockController.TryDockWindow(
				movingWindow,
				m_InputTracker.GetMousePos(),
				m_WindowInteraction.GetMoveStartMousePos(),
				m_Style);
		}
	}

	m_WindowInteraction.ReleasePointerIfMouseUp(m_InputTracker);

	// 8. ここから今フレームのBeginWindow生存申告を取り直す
	m_WindowManager.BeginFrame();
}

// -------------------------------------------------------------------------------
// フレーム終了
//
// 描画の重なり順をここで確定する
//	Dockウィンドウ → Floatingウィンドウ → ポップアップ → オーバーレイ
// 後ろに積んだものほど手前に描かれる
// -------------------------------------------------------------------------------
void EditorUI::Context::EndFrame()
{
	// ポップアップは通常のZ順とは別枠で最後に積むため、ここでは除外する
	const auto isPopupWindow = [this](EditorUI::Id _id)
	{
		return std::find(m_PopupDrawOrder.begin(), m_PopupDrawOrder.end(), _id) != m_PopupDrawOrder.end();
	};

	// DockWindowはレイアウト面として先に出力する
	for (EditorUI::Id id : m_WindowManager.GetWindowOrder())
	{
		if (m_WindowManager.IsActive(id) && m_DockController.IsDocked(id) && !isPopupWindow(id))
		{
			m_FrameContext.AppendDrawList(id);
		}
	}

	// FloatingWindowは通常Z-orderでDock面の前へ出力する
	for (EditorUI::Id id : m_WindowManager.GetWindowOrder())
	{
		if (m_WindowManager.IsActive(id) && !m_DockController.IsDocked(id) && !isPopupWindow(id))
		{
			m_FrameContext.AppendDrawList(id);
		}
	}

	// ポップアップは開いた順に、すべてのウィンドウより手前へ
	for (EditorUI::Id id : m_PopupDrawOrder)
	{
		m_FrameContext.AppendDrawList(id);
	}

	// ドッキングのプレビューなど、どのウィンドウにも属さないものを最前面へ
	m_FrameContext.AppendOverlayDrawList();
}

// -------------------------------------------------------------------------------
// 初期化・共有リソース
// -------------------------------------------------------------------------------
void EditorUI::Context::InitDockSpace(const EditorUI::Rect2D& _screenBounds)
{
	m_ScreenBounds = _screenBounds;
	m_DockController.Init(GetDockArea());
}

// -------------------------------------------------------------------------------
// 画面サイズの更新
//
// ドッキング領域の再計算をここで完結させる
// NewFrame のホバー判定より前にこの関数が呼ばれるため、
// 判定に使われる矩形は常にメニューバーを避けた正しいものになる
// -------------------------------------------------------------------------------
void EditorUI::Context::UpdateDockSpaceLayout(const EditorUI::Rect2D& _screenBounds)
{
	m_ScreenBounds = _screenBounds;
	m_DockController.UpdateLayout(GetDockArea());
}

void EditorUI::Context::SetDockAreaInsetTop(float _inset)
{
	m_DockAreaInsetTop = (std::max)(0.0f, _inset);

	// 指定が変わった時点で配り直しておく
	// 次のUpdateDockSpaceLayoutを待つと、1フレームだけ古い矩形が使われる
	m_DockController.UpdateLayout(GetDockArea());
}

EditorUI::Rect2D EditorUI::Context::GetDockArea() const
{
	return Rect2D
	{
		{ m_ScreenBounds.Min.x, m_ScreenBounds.Min.y + m_DockAreaInsetTop },
		m_ScreenBounds.Max
	};
}

const EditorUI::Rect2D& EditorUI::Context::GetScreenBounds() const
{
	return m_ScreenBounds;
}

// -------------------------------------------------------------------------------
// 既定レイアウトの構築
//
// ウィンドウ名からIdを求めるところだけがContextの仕事で、
// 木構造の操作はDockControllerへ委譲する
// -------------------------------------------------------------------------------
bool EditorUI::Context::IsDockSpaceEmpty() const
{
	return m_DockController.IsEmpty();
}

int EditorUI::Context::SplitDockArea(EditorUI::DockSplitDir _dir, float _ratio)
{
	return m_DockController.SplitScreen(_dir, _ratio);
}

int EditorUI::Context::FindDockLeafAt(const DirectX::XMFLOAT2& _point) const
{
	return m_DockController.FindLeafAt(_point);
}

void EditorUI::Context::DockWindow(std::string_view _title, int _leafId)
{
	// ウィンドウのIdは、BeginWindowと同じ「ルートスコープでのタイトルのハッシュ」
	// まだBeginWindowされていなくても、先にドッキング先を決められる
	m_DockController.DockWindowIntoLeaf(_leafId, m_IdStack.GetId(_title));
}

EditorUI::Rect2D EditorUI::Context::GetDockAreaBounds() const
{
	return m_DockController.GetDockAreaBounds();
}

int EditorUI::Context::GetRootDockLeafId() const
{
	return m_DockController.GetRootLeafId();
}

void EditorUI::Context::SetFont(EditorUI::Font* _pFont)
{
	m_pFont = _pFont;
}

EditorUI::Font* EditorUI::Context::GetFont() const
{
	return m_pFont;
}

// -------------------------------------------------------------------------------
// ウィンドウ開始
// 
// Windowの永続状態を取得し、このフレームだけ有効なWindowFrameを構築する
// この関数では主に次を構築する
// ・WindowIdと永続WindowState
// ・初回/強制配置
// ・Dock/Floatingのどちらとして描くか
// ・Window/Contentの矩形
// ・TitleBar/TabBarの操作
// ・Scroll入力
// ・Widgetが使用するContentOrigin/ClipRect
// 
// WindowStateはフレームをまたいで残る永続状態、WindowFrameは今フレームだけの描画・レイアウト情報
// -------------------------------------------------------------------------------
bool EditorUI::Context::BeginWindow(std::string_view _title, bool* _isOpen, EditorUI::WindowFlags _flags)
{
	if (_isOpen != nullptr && !*_isOpen)
	{ return false; }

	// -------------------------------------------------------------------------------
	// WindowIdと永続WindowStateを取得
	// -------------------------------------------------------------------------------

	// 現在のIdStackのスコープをseedとしてタイトルをHash化し、このWindowを識別する一意のIdを取得
	const EditorUI::Id id			= m_IdStack.GetId(_title);
	// 初回だけSetNextWindowPlacementを適用するために使用
	const bool isNewWindow			= !m_WindowManager.Contains(id);
	// Windowの永続状態を取得
	EditorUI::WindowState& state	= m_WindowManager.GetOrCreate(id);

	// タイトルはタイトルバーとタブの表示に使うため、毎フレーム最新に保つ
	state.Title = std::string(_title);

	// -------------------------------------------------------------------------------
	// 前フレームで発生したClose要求を呼び出し側に反映
	// ×ボタンが押された瞬間に*_isOpenを書き換えるのではなく、
	// WindowStateへCloseRequestedだけを記録し、次のBeginWindowで反映する
	// 安全なBeginWindowのタイミングまで1フレーム遅延させる
	// -------------------------------------------------------------------------------
	if (state.CloseRequested)
	{
		// Close要求は今回処理したため、消費する
		state.CloseRequested = false;

		if (_isOpen != nullptr)
		{
			// Contextからこのboolを通して閉じたことを通知
			*_isOpen = false;
			return false;
		}
	}

	// -------------------------------------------------------------------------------
	// SetNextWindowPlacementによる配置指定を反映
	// -------------------------------------------------------------------------------

	// 配置指定は「初回生成時」か「強制指定(ポップアップ)」のときだけ適用する
	// 毎フレームしてしまうと、Move/Resizeするたびに次フレームで元に戻ってしまうため。
	// Forcedの場合はPopupやMenuBarなど、毎フレーム位置を外部から決定するWindowなので、既存Windowにも適用
	if (m_NextWindow.Pending && (isNewWindow || m_NextWindow.Forced))
	{
		state.Position = m_NextWindow.Position;
		// 最低サイズ保証
		state.Size.x = (std::max)(60.0f, m_NextWindow.Size.x);
		state.Size.y = (std::max)(20.0f, m_NextWindow.Size.y);
	}
	// SetNextWindow系の処理は、次の1つのWindowだけに適用するため、BeginWindowで必ずfalseに戻す
	m_NextWindow.Pending = false;
	m_NextWindow.Forced  = false;

	// 今フレーム有効なWindowとして扱う
	m_WindowManager.MarkActive(id);
	// Window内部用のIdスコープへ入る
	// 先ほどのGetIdはWindow自身のIdを取得するだけで、IdStackの状態は変更していない。
	// ここでWindowIdをStackへPushすることで、以降このWindowで
	// GetId("Button")のようにWidgetIdを生成した場合には、実際には
	// RootSeet - Hash("Inspector") - InspectorのSeed - Hash("Button") - Inspector内部のButtonId
	// という階層的なIdになる
	// そのため、別Windowにも同名の"Button"が存在していても、Idが衝突しない
	// EndWindowで必ずPopして、このWindowスコープから抜ける
	m_IdStack.PushString(_title);

	// -------------------------------------------------------------------------------
	// 今フレーム専用のWindowFrameを生成
	// -------------------------------------------------------------------------------
	EditorUI::WindowFrame& frame = m_FrameContext.PushWindowFrame(state, _flags);

	// -------------------------------------------------------------------------------
	// Dock状態を取得
	// -------------------------------------------------------------------------------

	// ポップアップはドッキングの対象外なので、ドック情報を引かない
	const bool isPopup = HasFlag(_flags, EditorUI::WindowFlags::Popup);
	const EditorUI::DockWindowInfo dockInfo = (isPopup || HasFlag(_flags, EditorUI::WindowFlags::NoDock))
		? EditorUI::DockWindowInfo{}
		: m_DockController.GetWindowInfo(id);

	// DockLeafの非ActiveTabはWindow実体を持たない
	if (dockInfo.IsDocked && !dockInfo.IsActiveTab)
	{
		frame.SkippedEntirely	= true;
		frame.SkipContents		= true;
		return false;
	}

	// -------------------------------------------------------------------------------
	// Windowの基本矩形を確定
	// -------------------------------------------------------------------------------

	// DockWindowが保持しているDockControllerのBoundsを利用
	// FloatingWindowのWindowStateのPosition/Sizeを使用
	// さらにTitleBar/DockTabBar分を除いたContentRectも計算する
	SetupWindowGeometry(frame, state, _flags, dockInfo);

	// -------------------------------------------------------------------------------
	// Window全体の描画Clip開始
	// -------------------------------------------------------------------------------
	frame.Draw.PushClipRect(frame.WindowRect);

	// -------------------------------------------------------------------------------
	// Window背景/Border描画
	// -------------------------------------------------------------------------------
	if (!HasFlag(_flags, EditorUI::WindowFlags::NoBackground))
	{
		frame.Draw.AddRectFilled(frame.WindowRect, m_Style.ColorWindowBg);
		frame.Draw.AddRectOutline(frame.WindowRect, m_Style.ColorBorder, m_Style.BorderThickness);
	}

	// FloatingWindowだけが独立したTitleBarを持つ
	// DockWindowの場合はDockControllerがTabBarを描画するため、通常のTitleBarは表示しない
	const bool hasTitleBar = !frame.IsDocked && !HasFlag(_flags, EditorUI::WindowFlags::NoTitleBar);

	// -------------------------------------------------------------------------------
	// FloatingWindowのTitleBar
	// -------------------------------------------------------------------------------
	if (hasTitleBar)
	{
		// Window上端からChromeHeight分をTitleBar領域とする
		const EditorUI::Rect2D titleBarRect = MakeRect(frame.WindowRect.Min, { frame.WindowRect.Width(), frame.ChromeHeight });
		// Focus中のWindowはタイトルバーの色を変え、現在操作対象になっていることを視覚的に示す
		const bool focused = m_WindowManager.GetFocused() == id;
		frame.Draw.AddRectFilled(titleBarRect, focused ? m_Style.ColorTitleBarBgFocused : m_Style.ColorTitleBarBg);

		// Windowタイトル文字と×ボタンを描画。閉じる要求はStateに立て、次フレームで反映する
		DrawWindowChrome(frame, state, titleBarRect, _isOpen != nullptr);

		// -------------------------------------------------------------------------------
		// FloatingWindowの移動処理
		// -------------------------------------------------------------------------------
		if (!HasFlag(_flags, EditorUI::WindowFlags::NoMove))
		{
			// SplitterやWidgetがすでにMouse操作を取得している場合、WindowMoveが同じPointerを横取りしないようにする
			const bool allowCapture = !m_DockController.IsPointerBusy() && !m_WidgetInteractionState.IsAnyActive();
			const EditorUI::WindowMoveResult move = m_WindowInteraction.HandleTitleBarDrag(
				state, titleBarRect, m_InputTracker, allowCapture);

			// Drag開始時点でWindowをFocusし、FloatingWindowのz-orderでも最前面に移動させる
			if (move.Started)
			{
				m_WindowManager.SetFocused(id);
				m_WindowManager.BringToFron(id);
			}

			// -------------------------------------------------------------------------------
			// Dock候補Preview
			// FloatingWindowをDragしている間、Mouse位置に応じたDock候補領域を表示する
			// PreviewをWindow自身のDrawListに積むとWindowRectでClipされてしまい、画面端のDockPreview等が見えなくなる。
			// そのため、Windowに属さないOverlayDrawListへ描画する
			// -------------------------------------------------------------------------------
			if (move.Active && !HasFlag(_flags, EditorUI::WindowFlags::NoDock))
			{
				m_DockController.DrawPreview(
					m_FrameContext.GetOverlayDrawList(), m_InputTracker.GetMousePos(), m_Style);
			}
		}
	}
	// -------------------------------------------------------------------------------
	// DockWindowのTabBar
	// -------------------------------------------------------------------------------
	else if (frame.IsDocked)
	{
		// WindowMoveやWidget操作が進行中の場合は、DockTabのUndock操作を開始させない。
		const bool allowUndock = !m_WindowInteraction.IsBusy() && !m_WidgetInteractionState.IsAnyActive();

		// -------------------------------------------------------------------------------
		// WindowId → Windowタイトル変換
		// DockController側が必要としているのはこのWindowIdの表示名は何かという情報なので、ContextからCallbackとして渡す
		// -------------------------------------------------------------------------------
		const auto titleOf = [this](EditorUI::Id _windowId) -> std::string_view
		{
			const EditorUI::WindowState* pState = m_WindowManager.Find(_windowId);
			return (pState != nullptr) ? std::string_view(pState->Title) : std::string_view{};
		};

		// TabBarを描画し、
		// FocusされたWindow、閉じられたWindow、UndockされたWindow
		// を結果として受け取る
		const EditorUI::DockTabInteractionResult tabResult = m_DockController.DrawTabBar(
			frame, dockInfo.LeafId, frame.WindowRect, m_InputTracker, m_Style, allowUndock, m_pFont, titleOf);

		// DockController自身ではWindowManager等を操作せず、結果の反映はContext側で行う
		ApplyDockTabResult(tabResult);
	}

	// -------------------------------------------------------------------------------
	// MouseWheelによるScroll入力
	// -------------------------------------------------------------------------------
	if (!HasFlag(_flags, EditorUI::WindowFlags::NoScrollbar))
	{
		m_WindowInteraction.HandleScrollInput(state, frame.WindowRect, m_InputTracker);
	}

	// -------------------------------------------------------------------------------
	// Content領域のPaddingを確定
	// -------------------------------------------------------------------------------

	// NextPadding.Pendingが指定されていればその値を使用し、指定がなければStyleの標準WindowPaddingを使用する
	frame.Padding = m_NextPadding.Pending
		? m_NextPadding.Value
		: DirectX::XMFLOAT2{ m_Style.WindowPadding, m_Style.WindowPadding };
	// Padding指定も次のWindow1つだけに適用されるため、BeginWindowで消費する
	m_NextPadding.Pending = false;

	// -------------------------------------------------------------------------------
	// Widget配置開始位置を計算
	// -------------------------------------------------------------------------------

	// ContentRect左上からPadding分だけ内側をWidget配置の基準座標とする
	frame.ContentOrigin =
	{
		frame.ContentRect.Min.x + frame.Padding.x,
		frame.ContentRect.Min.y + frame.Padding.y
	};
	// 最初のWidgetはContentOriginから配置する
	// Widgetが追加されるたびLayout処理によってCursorPosが進んでいく
	frame.CursorPos = frame.ContentOrigin;

	// -------------------------------------------------------------------------------
	// Content領域用Clipを開始
	// 
	// 先ほどWindowRectのClipを積んでいるため、現在は
	// WindowRect.Clip -> ContentRect.Clip
	// というネストになる。
	// PushClipRectは親Clipとの交差領域を使用するため、
	// Widget側がさらにClipを積んでもWindowのContent領域より外には描画されない。
	// -------------------------------------------------------------------------------
	frame.Draw.PushClipRect(frame.ContentRect);
	// Collapsed状態ではWindow自体は存在されるが、内部Widgetの構築だけを省略する
	frame.SkipContents = state.Collapsed;
	return !frame.SkipContents;
}

// -------------------------------------------------------------------------------
// ウィンドウ終了
// -------------------------------------------------------------------------------
void EditorUI::Context::EndWindow()
{
	EditorUI::WindowFrame* frame = m_FrameContext.GetCurrentWindow();
	if (frame == nullptr || frame->pState == nullptr)
	{ return; }

	EditorUI::WindowState& state = *frame->pState;

	if (frame->SkippedEntirely)
	{
		m_FrameContext.PopWindowFrame();
		m_IdStack.Pop();
		return;
	}

	FinalizeScrollRange(*frame, state);

	// BeginWindowで積んだContentClip->WindowClipの順に戻す
	frame->Draw.PopClipRect();
	frame->Draw.PopClipRect();

	const bool allowWindowCapture = !m_DockController.IsPointerBusy() && !m_WidgetInteractionState.IsAnyActive();

	if (!HasFlag(frame->Flags, EditorUI::WindowFlags::NoScrollbar) && state.MaxScrollY > 0.0f)
	{
		m_WindowInteraction.DrawScrollbar(
			*frame, state, m_InputTracker, m_Style, allowWindowCapture);
	}

	if (!HasFlag(frame->Flags, EditorUI::WindowFlags::NoResize) && !frame->IsDocked)
	{
		// グリップの上にいる間は、斜めの両矢印で「引き伸ばせる」ことを示す
		if (m_WindowInteraction.DrawResizeGrip(
				state, *frame, m_InputTracker, m_Style, allowWindowCapture))
		{
			RequestMouseCursor(EditorUI::MouseCursor::ResizeNWSE);
		}
	}

	m_FrameContext.PopWindowFrame();
	m_IdStack.Pop();
}

void EditorUI::Context::RequestDestroyWindow(Id _windowId)
{
	m_WindowManager.RequestDestroy(_windowId);
}

void EditorUI::Context::SetNextWindowPlacement(const DirectX::XMFLOAT2& _position, const DirectX::XMFLOAT2& _size)
{
	m_NextWindow.Pending	= true;
	m_NextWindow.Forced		= false;
	m_NextWindow.Position	= _position;
	m_NextWindow.Size		= _size;
}

// -------------------------------------------------------------------------------
// 次のWindowだけContentPaddingを上書き
// 
// MenuBarやPopupのように通常Windowと異なる余白が必要な場合に使用する
// 指定はBeginWindow内で消費され、後続Windowへ持ち越さない。
// -------------------------------------------------------------------------------
void EditorUI::Context::SetNextWindowPadding(const DirectX::XMFLOAT2& _padding)
{
	m_NextPadding.Pending	= true;
	m_NextPadding.Value		= _padding;
}

// -------------------------------------------------------------------------------
// 次のWindowの配置を強制指定
// 
// Popupや固定MenuBarなど、既存のWindowStateが持つ位置よりも呼び出し側が毎フレーム決める位置を優先したい場合に使用
// -------------------------------------------------------------------------------
void EditorUI::Context::SetNextWindowPlacementForced(const DirectX::XMFLOAT2& _position, const DirectX::XMFLOAT2& _size)
{
	m_NextWindow.Pending	= true;
	m_NextWindow.Forced		= true;
	m_NextWindow.Position	= _position;
	m_NextWindow.Size		= _size;
}

// -------------------------------------------------------------------------------
// ポップアップ
// -------------------------------------------------------------------------------
void EditorUI::Context::OpenPopup(std::string_view _idLabel)
{
	OpenPopupAt(_idLabel, m_InputTracker.GetMousePos());
}

void EditorUI::Context::OpenPopupAt(std::string_view _idLabel, const DirectX::XMFLOAT2& _position)
{
	const EditorUI::Id popupId = m_IdStack.GetId(_idLabel);

	// 同じポップアップを開き直した場合は、位置だけ更新する
	if (PopupState* pExisting = FindPopup(popupId))
	{
		pExisting->Anchor = _position;
		return;
	}

	PopupState popup;
	popup.PopupId	= popupId;
	popup.Anchor	= _position;
	popup.Size		= { m_Style.MenuMinWidth, m_Style.MenuItemHeight };

	m_OpenPopups.push_back(popup);
}

bool EditorUI::Context::IsPopupOpen(std::string_view _idLabel) const
{
	return FindPopup(m_IdStack.GetId(_idLabel)) != nullptr;
}

// -------------------------------------------------------------------------------
// ポップアップの開始
//
// 中身のサイズは事前にはわからないため、前フレームに測った値で開く
// 開いた最初の1フレームだけ最小サイズになるが、2フレーム目以降は中身に一致する
// -------------------------------------------------------------------------------
bool EditorUI::Context::BeginPopup(std::string_view _idLabel)
{
	const EditorUI::Id popupId = m_IdStack.GetId(_idLabel);

	PopupState* pPopup = FindPopup(popupId);
	if (pPopup == nullptr)
	{ return false; }

	// 画面の外へはみ出す場合は、内側へ押し戻して全体が見えるようにする
	DirectX::XMFLOAT2 position = pPopup->Anchor;
	position.x = (std::min)(position.x, m_ScreenBounds.Max.x - pPopup->Size.x);
	position.y = (std::min)(position.y, m_ScreenBounds.Max.y - pPopup->Size.y);
	position.x = (std::max)(position.x, m_ScreenBounds.Min.x);
	position.y = (std::max)(position.y, m_ScreenBounds.Min.y);

	// ポップアップは毎フレーム位置とサイズを決め直すため、強制指定で開く
	SetNextWindowPlacementForced(position, pPopup->Size);

	const bool visible = BeginWindow(_idLabel, nullptr, EditorUI::WindowFlags::Menu);

	EditorUI::WindowFrame* pFrame = m_FrameContext.GetCurrentWindow();
	if (pFrame == nullptr || pFrame->pState == nullptr)
	{
		if (visible) { EndWindow(); }
		return false;
	}

	// FindPopupのポインタはBeginWindowの中で無効化されないが、
	// 念のためIdから引き直してから記録する
	if (PopupState* pCurrent = FindPopup(popupId))
	{
		pCurrent->Rect		= pFrame->WindowRect;
		pCurrent->WindowId	= pFrame->pState->WindowId;
	}

	m_PopupDrawOrder.push_back(pFrame->pState->WindowId);
	m_PopupStack.push_back(popupId);

	return visible;
}

// -------------------------------------------------------------------------------
// ポップアップの終了
// 積まれた中身の大きさを測り、次フレームのウィンドウサイズとして覚えておく
// -------------------------------------------------------------------------------
void EditorUI::Context::EndPopup()
{
	EditorUI::WindowFrame* pFrame = m_FrameContext.GetCurrentWindow();

	if (pFrame != nullptr && !m_PopupStack.empty())
	{
		if (PopupState* pPopup = FindPopup(m_PopupStack.back()))
		{
			// 中身 + 上下左右のパディングが、次に開くときのサイズになる
			pPopup->Size =
			{
				(std::max)(m_Style.MenuMinWidth, pFrame->ContentWidthUsed + m_Style.WindowPadding * 2.0f),
				(std::max)(m_Style.MenuItemHeight, pFrame->ContentHeight + m_Style.WindowPadding * 2.0f)
			};
		}
	}

	if (!m_PopupStack.empty())
	{
		m_PopupStack.pop_back();
	}

	EndWindow();
}

void EditorUI::Context::CloseCurrentPopup()
{
	if (m_PopupStack.empty())
	{
		CloseAllPopups();
		return;
	}

	const EditorUI::Id popupId = m_PopupStack.back();

	m_OpenPopups.erase(
		std::remove_if(m_OpenPopups.begin(), m_OpenPopups.end(),
			[popupId](const PopupState& _popup) { return _popup.PopupId == popupId; }),
		m_OpenPopups.end());
}

void EditorUI::Context::CloseAllPopups()
{
	m_OpenPopups.clear();
}

bool EditorUI::Context::IsAnyPopupHovered() const
{
	const DirectX::XMFLOAT2& mousePos = m_InputTracker.GetMousePos();

	for (const PopupState& popup : m_OpenPopups)
	{
		if (popup.Rect.IsValid() && popup.Rect.Contains(mousePos))
		{
			return true;
		}
	}
	return false;
}

// -------------------------------------------------------------------------------
// 範囲外クリックでポップアップを閉じる
//
// ネストしたメニューでは、クリックされた階層より深いものだけを閉じる
// -------------------------------------------------------------------------------
void EditorUI::Context::UpdatePopupDismiss()
{
	if (m_OpenPopups.empty())
	{ return; }

	// Escapeはいちばん手前のポップアップを1つ閉じる
	if (m_InputTracker.IsKeyPressed(EditorUI::Key::Escape))
	{
		m_OpenPopups.pop_back();
		return;
	}

	const bool clicked =
		m_InputTracker.IsMouseClicked(EditorUI::MouseButton::Mouse_Left) ||
		m_InputTracker.IsMouseClicked(EditorUI::MouseButton::Mouse_Right);

	if (!clicked)
	{ return; }

	const DirectX::XMFLOAT2& mousePos = m_InputTracker.GetMousePos();

	// 手前から見て、最初に見つかった「クリックを含むポップアップ」より深い階層を閉じる
	for (std::size_t i = m_OpenPopups.size(); i > 0; --i)
	{
		const PopupState& popup = m_OpenPopups[i - 1];

		if (popup.Rect.IsValid() && popup.Rect.Contains(mousePos))
		{
			m_OpenPopups.resize(i);
			return;
		}
	}

	// どのポップアップの上でもなければ、すべて閉じる
	m_OpenPopups.clear();
}

EditorUI::Context::PopupState* EditorUI::Context::FindPopup(EditorUI::Id _popupId)
{
	auto it = std::find_if(m_OpenPopups.begin(), m_OpenPopups.end(),
		[_popupId](const PopupState& _popup) { return _popup.PopupId == _popupId; });

	return (it != m_OpenPopups.end()) ? &(*it) : nullptr;
}

const EditorUI::Context::PopupState* EditorUI::Context::FindPopup(EditorUI::Id _popupId) const
{
	auto it = std::find_if(m_OpenPopups.begin(), m_OpenPopups.end(),
		[_popupId](const PopupState& _popup) { return _popup.PopupId == _popupId; });

	return (it != m_OpenPopups.end()) ? &(*it) : nullptr;
}

// -------------------------------------------------------------------------------
// スタイル・状態の問い合わせ
// -------------------------------------------------------------------------------
const EditorUI::Style& EditorUI::Context::GetStyle() const
{
	return m_Style;
}

void EditorUI::Context::SetStyle(const EditorUI::Style& _style)
{
	m_Style = _style;
}

EditorUI::IdStack& EditorUI::Context::GetIdStack()
{
	return m_IdStack;
}

const EditorUI::IdStack& EditorUI::Context::GetIdStack() const
{
	return m_IdStack;
}

EditorUI::WindowFrame* EditorUI::Context::GetCurrentWindow()
{
	return m_FrameContext.GetCurrentWindow();
}

const EditorUI::WindowFrame* EditorUI::Context::GetCurrentWindow() const
{
	return m_FrameContext.GetCurrentWindow();
}

EditorUI::DrawList& EditorUI::Context::GetOverlayDrawList()
{
	return m_FrameContext.GetOverlayDrawList();
}

bool EditorUI::Context::IsMouseOverAnyWindow() const
{
	return m_WindowManager.GetHovered() != 0;
}

EditorUI::Id EditorUI::Context::GetHoveredWindow() const
{
	return m_WindowManager.GetHovered();
}

// -------------------------------------------------------------------------------
// 今描いているウィンドウがマウスの直下にあるか
//
// ポップアップが開いている間は、その下のウィンドウに操作が漏れないようにする
// -------------------------------------------------------------------------------
bool EditorUI::Context::IsCurrentWindowHovered() const
{
	const EditorUI::WindowFrame* frame = m_FrameContext.GetCurrentWindow();
	if (frame == nullptr || frame->pState == nullptr)
	{ return false; }

	const EditorUI::Id currentId = frame->pState->WindowId;

	// ポップアップの中身は、Z順の判定に関係なく常に操作できる
	const bool isPopupWindow =
		std::find(m_PopupDrawOrder.begin(), m_PopupDrawOrder.end(), currentId) != m_PopupDrawOrder.end();

	if (isPopupWindow)
	{
		return frame->WindowRect.Contains(m_InputTracker.GetMousePos());
	}

	// 通常のウィンドウは、ポップアップに覆われていたら反応しない
	if (IsAnyPopupHovered())
	{ return false; }

	return m_WindowManager.GetHovered() == currentId;
}

const EditorUI::FrameOutput& EditorUI::Context::GetFrameOutput() const
{
	return m_FrameContext.GetOutput();
}

const DirectX::XMFLOAT2& EditorUI::Context::GetMousePos() const
{
	return m_InputTracker.GetMousePos();
}

DirectX::XMFLOAT2 EditorUI::Context::GetMouseDelta() const
{
	return m_InputTracker.GetMouseDelta();
}

float EditorUI::Context::GetMouseWheel() const
{
	return m_InputTracker.GetMouseWheel();
}

bool EditorUI::Context::IsMouseDown(EditorUI::MouseButton _button) const
{
	return m_InputTracker.IsMouseDown(_button);
}

bool EditorUI::Context::IsMouseClicked(EditorUI::MouseButton _button) const
{
	return m_InputTracker.IsMouseClicked(_button);
}

bool EditorUI::Context::IsMouseReleased(EditorUI::MouseButton _button) const
{
	return m_InputTracker.IsMouseReleased(_button);
}

bool EditorUI::Context::IsMouseDoubleClicked(EditorUI::MouseButton _button) const
{
	return m_InputTracker.IsMouseDoubleClicked(_button);
}

bool EditorUI::Context::IsKeyDown(EditorUI::Key _key) const
{
	return m_InputTracker.IsKeyDown(_key);
}

bool EditorUI::Context::IsKeyPressed(Key _key) const
{
	return m_InputTracker.IsKeyPressed(_key);
}

const std::wstring& EditorUI::Context::GetInputCharacters() const
{
	return m_InputTracker.GetInputCharacters();
}

float EditorUI::Context::GetDeltaTime() const
{
	return m_InputTracker.GetDeltaTime();
}

double EditorUI::Context::GetTime() const
{
	return m_InputTracker.GetTime();
}

// -------------------------------------------------------------------------------
// ウィジェット用の永続ストレージ
// -------------------------------------------------------------------------------
bool EditorUI::Context::GetStorageBool(EditorUI::Id _id, bool _defaultValue) const
{
	auto it = m_StorageBool.find(_id);
	return (it != m_StorageBool.end()) ? it->second : _defaultValue;
}

void EditorUI::Context::SetStorageBool(EditorUI::Id _id, bool _value)
{
	m_StorageBool[_id] = _value;
}

float EditorUI::Context::GetStorageFloat(EditorUI::Id _id, float _defaultValue) const
{
	auto it = m_StorageFloat.find(_id);
	return (it != m_StorageFloat.end()) ? it->second : _defaultValue;
}

void EditorUI::Context::SetStorageFloat(EditorUI::Id _id, float _value)
{
	m_StorageFloat[_id] = _value;
}

EditorUI::Id EditorUI::Context::GetActiveId() const
{
	return m_WidgetInteractionState.GetActiveId();
}

bool EditorUI::Context::IsActiveId(EditorUI::Id _id) const
{
	return m_WidgetInteractionState.IsActive(_id);
}

bool EditorUI::Context::IsAnyItemActive() const
{
	return m_WidgetInteractionState.IsAnyActive();
}

void EditorUI::Context::SetActive(EditorUI::Id _id)
{
	return m_WidgetInteractionState.SetActive(_id, m_InputTracker.GetMousePos());
}

void EditorUI::Context::ClearActiveId(EditorUI::Id _id)
{
	return m_WidgetInteractionState.ClearActive(_id);
}

void EditorUI::Context::KeepActiveIdAlive(EditorUI::Id _id)
{
	return m_WidgetInteractionState.KeepActive(_id);
}

EditorUI::Id EditorUI::Context::GetHoveredId() const
{
	return m_WidgetInteractionState.GetHoveredId();
}

bool EditorUI::Context::IsHoveredId(EditorUI::Id _id) const
{
	return m_WidgetInteractionState.IsHovered(_id);
}

void EditorUI::Context::SetHoveredId(EditorUI::Id _id)
{
	m_WidgetInteractionState.SetHovered(_id);
}

const DirectX::XMFLOAT2& EditorUI::Context::GetActiveIdClickPos() const
{
	return m_WidgetInteractionState.GetActiveClickPos();
}

double& EditorUI::Context::GetActiveDragAccumulator()
{
	return m_WidgetInteractionState.GetDragAccumulator();
}

EditorUI::TextEditState& EditorUI::Context::GetTextEditState()
{
	return m_WidgetInteractionState.GetTextEditState();
}

const EditorUI::TextEditState& EditorUI::Context::GetTextEditState() const
{
	return m_WidgetInteractionState.GetTextEditState();
}

bool EditorUI::Context::IsUiOperationActive() const
{
	return m_WidgetInteractionState.IsAnyActive() ||
		m_WindowInteraction.IsBusy() ||
		m_DockController.IsPointerBusy() ||
		!m_OpenPopups.empty();
}

// -------------------------------------------------------------------------------
// マウスカーソルの形の希望を立てる
// -------------------------------------------------------------------------------
void EditorUI::Context::RequestMouseCursor(EditorUI::MouseCursor _cursor)
{
	m_MouseCursor = _cursor;
}

// -------------------------------------------------------------------------------
// 今フレームに立った希望を返す（実際の切り替えはプラットフォーム層が行う）
// -------------------------------------------------------------------------------
EditorUI::MouseCursor EditorUI::Context::GetMouseCursor() const
{
	return m_MouseCursor;
}

bool EditorUI::Context::WantCaptureMouse() const
{
	// Drag中にMouseがWindow外へ出てもScene側へ入力を漏らさない
	return m_WidgetInteractionState.IsAnyActive() ||
		m_WindowInteraction.IsBusy() ||
		m_DockController.IsPointerBusy() ||
		!m_OpenPopups.empty() ||
		IsMouseOverAnyWindow();
}

bool EditorUI::Context::WantCaptureKeyboard() const
{
	return m_WidgetInteractionState.GetTextEditState().Widget != 0;
}

// -------------------------------------------------------------------------------
// 内部処理
// -------------------------------------------------------------------------------
void EditorUI::Context::ProcessPendingDestroyWindows()
{
	const std::vector<EditorUI::Id> pending = m_WindowManager.ConsumePendingDestroy();

	for (EditorUI::Id windowId : pending)
	{
		// Window削除はDock/PointerCapture/StateRegistryを同時に整合させる必要があるため
		// その順序だけをContextが調停する
		m_DockController.RemoveWindow(windowId);
		m_WindowInteraction.CancelForWindow(windowId);
		m_WindowManager.DestroyWindowImmediate(windowId);
	}
}

bool EditorUI::Context::TryGetWindowRect(EditorUI::Id _windowId, EditorUI::Rect2D& _outRect) const
{
	const EditorUI::WindowState* state = m_WindowManager.Find(_windowId);
	if (state == nullptr || !state->Active)
	{ return false; }

	const EditorUI::DockWindowInfo dock = m_DockController.GetWindowInfo(_windowId);
	if (dock.IsDocked)
	{
		if (!dock.IsActiveTab)
		{ return false; }

		_outRect = dock.Bounds;
		return true;
	}

	_outRect = MakeRect(state->Position, state->Size);
	return true;
}

EditorUI::Id EditorUI::Context::FindTopmostWindowAt(const DirectX::XMFLOAT2& _point) const
{
	// ポップアップは常に最前面なので、通常のZ順より先に調べる
	for (std::size_t i = m_OpenPopups.size(); i > 0; --i)
	{
		const PopupState& popup = m_OpenPopups[i - 1];
		if (popup.WindowId != 0 && popup.Rect.IsValid() && popup.Rect.Contains(_point))
		{
			return popup.WindowId;
		}
	}

	const auto& order = m_WindowManager.GetWindowOrder();
	for (auto it = order.rbegin(); it != order.rend(); ++it)
	{
		Rect2D rect{};
		if (TryGetWindowRect(*it, rect) && rect.Contains(_point))
		{
			return *it;
		}
	}
	return 0;
}

void EditorUI::Context::HandleWindowFocusClick()
{
	if (!m_InputTracker.IsMouseClicked(EditorUI::MouseButton::Mouse_Left))
	{ return; }

	// SplitterがClickを取得したフレームはWindowFocusへ流さない
	if (m_DockController.IsSplitterHovered())
	{ return; }

	// ポップアップの上のクリックは、背後のウィンドウのフォーカスを変えない
	if (IsAnyPopupHovered())
	{ return; }

	const EditorUI::Id hovered = m_WindowManager.GetHovered();
	if (hovered != 0)
	{
		m_WindowManager.SetFocused(hovered);
		m_WindowManager.BringToFron(hovered);
	}
}

void EditorUI::Context::ApplyDockTabResult(const EditorUI::DockTabInteractionResult& _result)
{
	if (_result.FocusWindow != 0)
	{
		m_WindowManager.SetFocused(_result.FocusWindow);
		m_WindowManager.BringToFron(_result.FocusWindow);
	}

	// タブの×が押された場合は、閉じる要求を立てるだけにする
	// 実際に閉じるのは、次フレームのBeginWindowで呼び出し側のbool*へ反映したとき
	if (_result.ClosedWindow != 0)
	{
		if (EditorUI::WindowState* pState = m_WindowManager.Find(_result.ClosedWindow))
		{
			pState->CloseRequested = true;
		}
	}

	if (_result.UndockedWindow == 0)
	{ return; }

	EditorUI::WindowState* state = m_WindowManager.Find(_result.UndockedWindow);
	if (state == nullptr)
	{ return; }

	m_WindowInteraction.BeginMoveFromDock(
		*state, _result.PreviousLeafBounds, _result.PressedOffset, _result.PressedMousePos, m_InputTracker.GetMousePos());
}

void EditorUI::Context::SetupWindowGeometry(
	EditorUI::WindowFrame&			_frame,
	const EditorUI::WindowState&	_state,
	EditorUI::WindowFlags			_flags,
	const EditorUI::DockWindowInfo& _dockInfo)
{
	_frame.IsDocked = _dockInfo.IsDocked;
	_frame.WindowRect = _dockInfo.IsDocked
		? _dockInfo.Bounds
		: MakeRect(_state.Position, _state.Size);

	const bool hasTitleBar = !_frame.IsDocked && !HasFlag(_flags, EditorUI::WindowFlags::NoTitleBar);
	_frame.ChromeHeight = (hasTitleBar || _frame.IsDocked)
		? m_Style.TitleBarHeight
		: 0.0f;

	_frame.ContentRect = MakeRect(
		{ _frame.WindowRect.Min.x, _frame.WindowRect.Min.y + _frame.ChromeHeight },
		{ _frame.WindowRect.Width(), (std::max)(0.0f, _frame.WindowRect.Height() - _frame.ChromeHeight) });
}

// -------------------------------------------------------------------------------
// タイトルバーの中身（名前と×ボタン）を描く
//
// タイトルの文字は「そのウィンドウが何なのか」を示す最低限の情報なので、
// ウィジェットではなくContextが必ず描く
// -------------------------------------------------------------------------------
void EditorUI::Context::DrawWindowChrome(
	EditorUI::WindowFrame&	_frame,
	EditorUI::WindowState&	_state,
	const EditorUI::Rect2D&	_titleBarRect,
	bool					_hasCloseButton)
{
	// ×ボタンの領域。閉じられないウィンドウでは幅0として扱う
	const float closeSize = _hasCloseButton ? m_Style.TabCloseButtonSize : 0.0f;

	const EditorUI::Rect2D closeRect = MakeRect(
		{
			_titleBarRect.Max.x - m_Style.FramePaddingX - closeSize,
			_titleBarRect.Min.y + (_titleBarRect.Height() - closeSize) * 0.5f
		},
		{ closeSize, closeSize });

	// -------------------------------------------------------------------------------
	// タイトル文字
	// -------------------------------------------------------------------------------
	if (m_pFont != nullptr && !_state.Title.empty())
	{
		const std::wstring wide = StringUtil::Utf8ToWide(_state.Title);

		const DirectX::XMFLOAT2 textPos
		{
			_titleBarRect.Min.x + m_Style.FramePaddingX,
			_titleBarRect.Min.y + (_titleBarRect.Height() - m_pFont->GetLineHeight()) * 0.5f
		};

		// ×ボタンに重ならないところまでで切る
		const float textRight = _hasCloseButton ? (closeRect.Min.x - 4.0f) : (_titleBarRect.Max.x - m_Style.FramePaddingX);

		_frame.Draw.PushClipRect({ _titleBarRect.Min, { textRight, _titleBarRect.Max.y } });
		_frame.Draw.AddText(textPos, m_Style.ColorTextBright, wide, *m_pFont);
		_frame.Draw.PopClipRect();
	}

	// -------------------------------------------------------------------------------
	// ×ボタン
	// -------------------------------------------------------------------------------
	if (!_hasCloseButton)
	{ return; }

	const bool hovered = closeRect.Contains(m_InputTracker.GetMousePos()) &&
		m_WindowManager.GetHovered() == _state.WindowId &&
		!IsAnyPopupHovered();

	if (hovered)
	{
		_frame.Draw.AddRectFilled(closeRect, m_Style.ColorButtonActive);
	}

	const float inset = closeRect.Width() * 0.28f;
	_frame.Draw.AddRectFilled(
		{
			{ closeRect.Min.x + inset, closeRect.Min.y + inset },
			{ closeRect.Max.x - inset, closeRect.Max.y - inset }
		},
		hovered ? m_Style.ColorTextBright : m_Style.ColorTextMuted);

	if (hovered && m_InputTracker.IsMouseClicked(EditorUI::MouseButton::Mouse_Left))
	{
		_state.CloseRequested = true;
	}
}

void EditorUI::Context::FinalizeScrollRange(EditorUI::WindowFrame& _frame, EditorUI::WindowState& _state)
{
	if (HasFlag(_frame.Flags, EditorUI::WindowFlags::NoScrollbar))
	{
		_state.MaxScrollY	= 0.0f;
		_state.Scroll.y		= 0.0f;
		return;
	}

	const float visibleHeight = (std::max)(1.0f, _frame.ContentRect.Height() - _frame.Padding.y * 2.0f);

	_state.MaxScrollY = (std::max)(0.0f, _frame.ContentHeight - visibleHeight);
	_state.Scroll.y = std::clamp(_state.Scroll.y, 0.0f, _state.MaxScrollY);
}
