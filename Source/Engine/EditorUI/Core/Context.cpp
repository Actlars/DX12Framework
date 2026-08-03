#include "Context.h"


EditorUI::Context::Context()
{
	// コンストラクタでスタイルの初期値をコピー
	m_Style = GetDefaultStyle();
}

EditorUI::Context::~Context()
{ /* DO_NOTHING */ }

void EditorUI::Context::InitDockSpace(const Rect2D & _screenBounds)
{
	m_DockSpace.Init(_screenBounds);
	m_DockSpaceInitialized = true;
}


void EditorUI::Context::UpdateDockSpaceLayout(const Rect2D& _screenBounds)
{
	if (m_DockSpaceInitialized)
	{
		m_DockSpace.UpdateLayout(_screenBounds);
	}
}

// -------------------------------------------------------------------------------
// IDスタックとこのフレームで生きているWindowFrameをクリアし、まっさらな状態に戻す
// -------------------------------------------------------------------------------
void EditorUI::Context::NewFrame(const InputState& _input)
{
	// 前フレームの入力をm_PrevInputに退避してから、新しい入力で上書きする
	m_PrevInput = m_Input;
	m_Input		= _input;

	m_IdStack.Clear();							// 現在地を番兵まで巻き戻す
	m_ActiveWindowFrame.clear();				// 前フレームのWindowFrameデータを破棄
	m_CompositedFrame.WindowDrawLists.clear();	// Render層に渡した参照も破棄

	// 全ウィンドウのActiveフラグを戻す
	// このフレームでBeginWindowされたものだけが、後でtrueに立て直される
	for (auto& [id, state] : m_WindowStates)	// unordered_mapの各要素をkeyとvalueに分解して受け取る書き方
	{
		state.Active = false;
	}

	// 左ボタンが離された瞬間を検出する。この時点ではまだm_DraggedWindowを
	// リセットしていないのでドロップ判定に使える
	const bool leftReleased = m_PrevInput.MouseDown[static_cast<int>(MouseButton::Mouse_Left)] &&
		!m_Input.MouseDown[static_cast<int>(MouseButton::Mouse_Left)];

	if (leftReleased && m_DraggedWindow != 0)
	{
		auto it = m_WindowStates.find(m_DraggedWindow);
		if (it != m_WindowStates.end())
		{
			HandleDockDrop(it->second);
		}
	}

	// マウスの左ボタンが離されていれば、ドラッグ中のウィンドウを強制的になしにする
	if (!m_Input.MouseDown[static_cast<int>(MouseButton::Mouse_Left)])
	{
		m_DraggedWindow				= 0;
		m_ResizeWindow				= 0;
		m_DraggedScrollbarWindow	= 0;

		// タグドラッグ候補も解除
		m_PressedDockTabWindow		= 0;
		m_PressedDockTabLeaf		= -1;
	}
}

// -------------------------------------------------------------------------------
// z-orderの順番通りにBeginWindowされた各ウィンドウのDrawListをRender層向けに並べる
// -------------------------------------------------------------------------------
void EditorUI::Context::EndFrame()
{
	// BeginWindowを読んだ後にEndWindowを呼び忘れていないかチェック

	// z-order順（背面→前面）にDrawListをRender層向けへ並べる
	for (Id id : m_WindowOrder)	// m_WindowOrderは表示順だけを覚えている配列データ
	{
		auto stateId = m_WindowStates.find(id);
		// 今フレームBeginされなかったウィンドウはスキップする
		if (stateId == m_WindowStates.end() || !stateId->second.Active)
		{
			continue;
		}

		// m_WindowOrderにはIdしか入っていないので、実際のDrawListを持つWindowFrameを
		// m_ActiveWindowFrameの中から線形探索して探す
		for (auto& frame : m_ActiveWindowFrame)
		{
			if (frame->pState->WindowId == id)
			{
				// 見つけたIdをもとにDrawListにpush_back
				m_CompositedFrame.WindowDrawLists.push_back(&frame->Draw);
				break;
			}
		}
	}
}

// -------------------------------------------------------------------------------
// 指定した名前のウィンドウを開始する
// -------------------------------------------------------------------------------
bool EditorUI::Context::BeginWindow(std::string_view _title, bool* _isOpen, WindowFlags _flags)
{

	if (_isOpen && !*_isOpen)
	{
		return false;
	}

	Id id = m_IdStack.GetId(_title);					// 現在のスコープを踏まえて、このウィンドウのIdを計算
	WindowState& state = GetOrCreateWindowState(id);	// 初回なら新規作成、２回目以降は既存を取得
	state.Active = true;								// 今フレーム参照されたフラグをtrue
	m_IdStack.PushString(_title);						// 以降このBeginWindow～EndWindowの間、現在地をこのウィンドウの中に移す

	// このフレーム限りの作業データ(WindowFrame)を新規作成する
	// make_uniqueで作成した所有権をm_ActiveWindowFrameに移し
	// 生のポインタ(Frame)は所有はしないが今すぐ使うための参照として保持する
	auto framePtr		= std::make_unique<WindowFrame>();
	WindowFrame* frame	= framePtr.get();	// unique_ptrから生ポインタを見る（所有権は移動しない）
	frame->pState		= &state;
	frame->Flags		= _flags;
	frame->Draw.Reset();								// 前回このWindowFrameが使われたときの頂点/インデックスが残っていないことの保証
	m_ActiveWindowFrame.push_back(std::move(framePtr));	// ここで所有者がframePtrからm_ActiveWindowFrameに移動する
	m_CurrentWindow = frame;							// 今Begin中のウィンドウとして記録

	// -------------------------------------------------------------------------------
	// ドッキング状態の判定
	// -------------------------------------------------------------------------------
	int leafId				= -1;
	bool isActiveTab		= true;
	const DockNode* leaf	= nullptr;

	if (m_DockSpaceInitialized)
	{
		leafId = m_DockSpace.FindLeafOwning(id);
		if (leafId != -1)
		{
			leaf = m_DockSpace.GetNode(leafId);
			if (leaf != nullptr && leaf->Windows.size() > 1)
			{
				auto it = std::find(leaf->Windows.begin(), leaf->Windows.end(), id);
				const int myIndex = static_cast<int>(std::distance(leaf->Windows.begin(), it));
				isActiveTab = (myIndex == leaf->ActiveTabIndex);
			}
		}
	}

	// 非アクティブなタブは何も描画せずここで終了する
	if (leafId != -1 && !isActiveTab)
	{
		frame->SkippedEntirely	= true;
		frame->SkipContents		= true;
		return false;
	}

	// 描画する矩形 : ドッキング済みならLeafの領域、フローティングなら自前のPosition/Size
	Rect2D windowRect = (leafId != -1) ? leaf->Bounds : MakeRect(state.Position, state.Size);

	bool hasTitleBar = !HasFlag(_flags, WindowFlags::NoTitleBar) && (leafId == -1);		// NoTitleBarフラグが立っていなければタイトルバーを描く
	const bool hasTabBar = (leafId != -1);	// ドッキング済みは1枚でも常にタブバー領域を持つ
	float titleBarHeight = (hasTitleBar || hasTabBar) ? m_Style.TitleBarHeight : 0.0f;

	// -------------------------------------------------------------------------------
	// ウィンドウ全体のクリップ
	// -------------------------------------------------------------------------------
	// ウィンドウ全体(背景＋枠線)を描画コマンドに積む
	//Rect2D windowRect = MakeRect(state.Position, state.Size);
	frame->Draw.PushClipRect(windowRect);

	// 背景
	frame->Draw.AddRectFilled(windowRect, m_Style.ColorWindowBg);
	// 枠線
	frame->Draw.AddRectOutline(windowRect, m_Style.ColorBorder, m_Style.BorderThickness);

	// タイトルバー
	if (hasTitleBar)
	{
		Rect2D	titleBarRect	= MakeRect(state.Position, { state.Size.x, titleBarHeight });
		bool	focused			= (m_FocusedWindow == id);	// 自分がフォーカス中のウィンドウかどうかで色を変える
		frame->Draw.AddRectFilled(titleBarRect, focused ? m_Style.ColorTitleBarBgFocused : m_Style.ColorTitleBarBg);

		// NoMoveフラグが立っていなければ、タイトルバーのドラッグ判定を行う
		if (!HasFlag(_flags, WindowFlags::NoMove))
		{
			HandleTitleBarDrag(*frame, state, titleBarRect);
		}
	}
	else if (hasTabBar)
	{
		DrawTabBar(*frame, *leaf, leafId, windowRect);
	}
	
	// ホイール入力をここで反映する
	// 前フレーム終了時点のMaxScrollYを基準にクランプするため、今フレームのコンテンツがまだ確定していなくても計算できる
	if (!HasFlag(_flags, WindowFlags::NoScrollbar))
	{
		HandleScrollInput(state, windowRect);
	}

	// コンテンツ領域（ウィジェットを実際に置いていく場所）の開始位置を計算する
	frame->ContentOrigin = { windowRect.Min.x + m_Style.WindowPadding,
								windowRect.Min.y + titleBarHeight + m_Style.WindowPadding };
	frame->CursorPos = frame->ContentOrigin;	// 仮想座標もここから始まる

	// コンテンツ領域はウィンドウ矩形でクリップする。Widgets層はこの中でAddRect/AddTextを積む
	Rect2D contentClip = MakeRect(
		{ windowRect.Min.x, windowRect.Min.y + titleBarHeight },
		{ windowRect.Width(), windowRect.Height() - titleBarHeight});

	// ウィジェット用クリップ
	frame->Draw.PushClipRect(contentClip);

	frame->SkipContents = state.Collapsed;
	return !frame->SkipContents;
}

// -------------------------------------------------------------------------------
// クリップスタックを戻し、Begin中フラグを解除し、IDスタックの現在地を１段戻す
// -------------------------------------------------------------------------------
void EditorUI::Context::EndWindow()
{
	WindowFrame& frame = *m_CurrentWindow;
	WindowState& state = *frame.pState;

	// 非アクティブタブは何もPush/Drawしていないので、対になる後始末も行わない
	if (frame.SkippedEntirely)
	{
		m_CurrentWindow = nullptr;
		m_IdStack.Pop();
		return;
	}

	const bool isDocked = m_DockSpaceInitialized && (m_DockSpace.FindLeafOwning(state.WindowId) != -1);

	// 今フレームで実際に置かれたウィジェットの高さが確定したので
	// スクロール可能な最大量をここで確定させ、前フレームの仮の値を補正する
	if (!HasFlag(frame.Flags, WindowFlags::NoScrollbar) && !isDocked)
	{
		const float titleBarHeight	= HasFlag(frame.Flags, WindowFlags::NoTitleBar) ? 0.0f : m_Style.TitleBarHeight;
		const float visibleHeight	= state.Size.y - titleBarHeight - m_Style.WindowPadding * 2.0f;

		state.MaxScrollY = (std::max)(0.0f, frame.ContentHeight - visibleHeight);
		state.Scroll.y = std::clamp(state.Scroll.y, 0.0f, state.MaxScrollY);
	}

	frame.Draw.PopClipRect();	// コンテンツ用クリップをここで戻す
	frame.Draw.PopClipRect();	// ウィンドウ全体用クリップをここで戻す

	// スクロールバー・リサイズグリップはウィンドウのクローム（枠）なので
	// コンテンツ用クリップの外で描く
	if (!HasFlag(frame.Flags, WindowFlags::NoScrollbar) && state.MaxScrollY > 0.0f)
	{
		DrawScrollbar(frame, state);
	}

	if (!HasFlag(frame.Flags, WindowFlags::NoResize) && !isDocked)
	{
		HandleResizeDrag(state, frame);
	}

	m_CurrentWindow = nullptr;				// Begin中の状態を解除
	m_IdStack.Pop();						// 現在地を親スコープに戻す
}

// -------------------------------------------------------------------------------
// ウィンドウ上かチェック
// -------------------------------------------------------------------------------
bool EditorUI::Context::IsMouseOverAnyWindow() const
{
	// 前面から調べる
	for (auto it = m_WindowOrder.rbegin();
		it != m_WindowOrder.rend();
		++it)
	{
		const Id id = *it;

		auto stateIt = m_WindowStates.find(id);
		if (stateIt == m_WindowStates.end())
		{
			continue;
		}

		const WindowState& state = stateIt->second;

		// 今フレームで表示されていないウィンドウは対象外
		if (!state.Active)
		{
			continue;
		}

		const Rect2D windowRect = MakeRect(state.Position, state.Size);

		if (windowRect.Contains(m_Input.MousePos))
		{
			return true;
		}
	}

	return false;
}

// -------------------------------------------------------------------------------
// 矩形内でのマウスの相対位置から４辺のうちどこに一番近いかを判定する
// -------------------------------------------------------------------------------
EditorUI::DockSplitDir EditorUI::Context::ComputeDropZone(const Rect2D& _leafBounds, const DirectX::XMFLOAT2& _mousePos) const
{
	if (!_leafBounds.Contains(_mousePos))
	{
		return DockSplitDir::None;
	}

	const float relX = (_mousePos.x - _leafBounds.Min.x) / _leafBounds.Width();
	const float relY = (_mousePos.y - _leafBounds.Min.y) / _leafBounds.Height();

	constexpr float kMargin = 0.25f;	// 端から25%以内にいれば分割、それ以外は中央(タブ)扱い

	const float distLeft	= relX;
	const float distRight	= 1.0f - relX;
	const float distTop		= relY;
	const float distBottom	= 1.0f - relY;

	const float minDist = (std::min)({ distLeft,distRight,distTop,distBottom });

	if (minDist > kMargin)
	{
		return DockSplitDir::Center;	// 中央タブ扱い
	}
	if (minDist == distLeft)	{ return DockSplitDir::Left; }
	if (minDist == distRight)	{ return DockSplitDir::Right; }
	if (minDist == distTop)		{ return DockSplitDir::Top; }

	return DockSplitDir::Bottom;
}

// -------------------------------------------------------------------------------
// ドッキングを確定させる
// -------------------------------------------------------------------------------
void EditorUI::Context::HandleDockDrop(WindowState& _state)
{
	if (!m_DockSpaceInitialized)
	{
		return;
	}

	// ほぼ動いていない場合はドッキング判定をしない
	constexpr float kDragThreshold = 5.0f;	// 5px未満の移動はクリックとして扱う
	const float dx = m_Input.MousePos.x - m_DragStartMousePos.x;
	const float dy = m_Input.MousePos.y - m_DragStartMousePos.y;
	if ((dx * dx + dy * dy) < (kDragThreshold * kDragThreshold))
	{ return; }

	const int leafId = m_DockSpace.FindLeafAt(m_Input.MousePos);
	if (leafId == -1)
	{ return; }	// ドックスペースの範囲外で話した場合はフローティングのまま

	const DockNode* leaf = m_DockSpace.GetNode(leafId);
	if (leaf == nullptr) 
	{ return; }

	// 空のLeafは分割せず、そのまま使用する
	if (!m_DockSpace.IsLeafOccupied(leafId))
	{
		m_DockSpace.DockWindowIntoLeaf(leafId, _state.WindowId);
		return;
	}

	const DockSplitDir dir = ComputeDropZone(leaf->Bounds, m_Input.MousePos);
	if (dir == DockSplitDir::None) 
	{ return; }

	if (dir == DockSplitDir::Center)
	{
		m_DockSpace.DockWindowIntoLeaf(leafId, _state.WindowId);
		return;
	}

	const int newLeafId = m_DockSpace.SplitLeaf(leafId, dir);
	if (newLeafId != -1)
	{
		m_DockSpace.DockWindowIntoLeaf(newLeafId, _state.WindowId);
	}
}

// -------------------------------------------------------------------------------
// ドッキングされたLeaf内のタブを描く
// -------------------------------------------------------------------------------
void EditorUI::Context::DrawTabBar(WindowFrame& _frame, const DockNode& _leaf, int _leafId, const Rect2D& _windowRect)
{
	const float tabHeight	= m_Style.TitleBarHeight;
	Rect2D tabBarRect		= MakeRect(_windowRect.Min, { _windowRect.Width(), tabHeight });
	_frame.Draw.AddRectFilled(tabBarRect, m_Style.ColorScrollbarBg);

	constexpr float kTabWidth			= 100.0f;
	constexpr float kUndockThreshold	= 5.0f;
	float tabX							= _windowRect.Min.x;

	const int	leftButton	= static_cast<int>(MouseButton::Mouse_Left);
	const bool	leftDown	= m_Input.MouseDown[leftButton];
	const bool	justPressed = leftDown && !m_PrevInput.MouseDown[leftButton];

	// -------------------------------------------------------------------------------
	// タブの描画と押下判定
	// -------------------------------------------------------------------------------
	for (size_t i = 0; i < _leaf.Windows.size(); ++i)
	{
		const Id tabWindowId	= _leaf.Windows[i];
		Rect2D tabRect			= MakeRect({ tabX, _windowRect.Min.y }, { kTabWidth, tabHeight });
		const bool isActive		= (static_cast<int>(i) == _leaf.ActiveTabIndex);
		const bool hovered		= tabRect.Contains(m_Input.MousePos);

		const Color32 tabColor = isActive ? m_Style.ColorTitleBarBgFocused
								: hovered ? m_Style.ColorButtonHovered
								: m_Style.ColorTitleBarBg;
		_frame.Draw.AddRectFilled(tabRect, tabColor);
		_frame.Draw.AddRectOutline(tabRect, m_Style.ColorBorder, m_Style.BorderThickness);

		// タブを押した瞬間
		if (hovered && justPressed)
		{
			// 押したタブをアクティブ化する
			m_DockSpace.SetActiveTab(_leafId, static_cast<int>(i));

			// ドラッグ候補として記録
			m_PressedDockTabWindow		= tabWindowId;
			m_PressedDockTabLeaf		= _leafId;
			m_PressedDockTabMousePos	= m_Input.MousePos;

			// leaf左上からクリック位置までの差を保存
			m_PressedDockTabOffset =
			{
				m_Input.MousePos.x - _windowRect.Min.x,
				m_Input.MousePos.y - _windowRect.Min.y
			};

			m_FocusedWindow = tabWindowId;
			BringToFront(tabWindowId);
		}

		tabX += kTabWidth;
	}

	// -------------------------------------------------------------------------------
	// 押したタブを一定距離以上動かしたらアンドック
	// -------------------------------------------------------------------------------
	if(m_PressedDockTabWindow		== 0		||
		m_PressedDockTabLeaf		!= _leafId	||
		!leftDown								||
		m_DraggedWindow				!= 0		||
		m_ResizeWindow				!= 0		||
		m_DraggedScrollbarWindow != 0) 
	{
		return;
	}

	const float dx = m_Input.MousePos.x - m_PressedDockTabMousePos.x;
	const float dy = m_Input.MousePos.y - m_PressedDockTabMousePos.y;

	const float distanceSquared		= dx * dx + dy * dy;
	const float thresholdSquared	= kUndockThreshold * kUndockThreshold;

	// まだドラッグとみなせる距離まで動いていない
	if (distanceSquared < thresholdSquared)
	{ return; }

	const Id draggedWindowId = m_PressedDockTabWindow;

	auto stateIt = m_WindowStates.find(draggedWindowId);

	if (stateIt == m_WindowStates.end())
	{
		m_PressedDockTabWindow	= 0;
		m_PressedDockTabLeaf	= -1;
		return;
	}

	WindowState& draggedState = stateIt->second;

	// -------------------------------------------------------------------------------
	// ドッキング中のLeafサイズをフローティング状態へ引き継ぐ
	// -------------------------------------------------------------------------------
	draggedState.Position = _leaf.Bounds.Min;

	draggedState.Size = { _leaf.Bounds.Width(), _leaf.Bounds.Height() };

	// 通常のタイトルバードラッグと同じ状態へ移行
	m_DraggedWindow		= draggedWindowId;
	m_DragOffset		= m_PressedDockTabOffset;
	m_DragStartMousePos = m_PressedDockTabMousePos;

	m_FocusedWindow = draggedWindowId;
	BringToFront(draggedWindowId);

	// -------------------------------------------------------------------------------
	// DockSpaceのLeafからウィンドウを外す
	// １枚しかなければMergeUpIfEmptyによってツリーも縮約される
	// -------------------------------------------------------------------------------
	m_DockSpace.UndockWindow(draggedWindowId);

	// 現在のマウス位置に合わせて移動
	draggedState.Position = { m_Input.MousePos.x - m_DragOffset.x, m_Input.MousePos.y - m_DragOffset.y };

	// ドラッグ候補状態は終了
	m_PressedDockTabWindow = 0;
	m_PressedDockTabLeaf = -1;

	// アンドック直後のフレームでもドッキング先を表示
	DrawDockPreview(_frame);

	return;
}

// -------------------------------------------------------------------------------
// ドロップ先をハイライト表示にする
// -------------------------------------------------------------------------------
void EditorUI::Context::DrawDockPreview(WindowFrame& _frame)
{
	const int leafId = m_DockSpace.FindLeafAt(m_Input.MousePos);
	if (leafId == -1) 
	{ return; }

	const DockNode* leaf = m_DockSpace.GetNode(leafId);
	if (leaf == nullptr) 
	{ return; }

	// 空のLeafなら、領域全体へのドッキングとして表示
	if (!m_DockSpace.IsLeafOccupied(leafId))
	{
		_frame.Draw.AddRectFilled(leaf->Bounds, 0x804A6A9Cu);
		return;
	}

	const DockSplitDir dir = ComputeDropZone(leaf->Bounds, m_Input.MousePos);
	if (dir == DockSplitDir::None)
	{ return; }

	Rect2D preview = leaf->Bounds;
	switch (dir)
	{
	case DockSplitDir::Left:	preview.Max.x = preview.Min.x + preview.Width() * 0.3f; break;
	case DockSplitDir::Right:	preview.Min.x = preview.Max.x - preview.Width() * 0.3f; break;
	case DockSplitDir::Top:		preview.Max.y = preview.Min.y + preview.Height() * 0.3f; break;
	case DockSplitDir::Bottom:	preview.Min.y = preview.Max.y - preview.Height() * 0.3f; break;
	case DockSplitDir::Center:break;	// leaf全体をそのままハイライト
	default:return;
	}

	// 半透明のまま青。ドラッグ中のウィンドウが最前面にいるため、このDrawListに積めばプレビューも最前面に出る
	_frame.Draw.AddRectFilled(preview, 0x804A6A9Cu);
}

// -------------------------------------------------------------------------------
// 指定したIdに対するWindowStateを取得する
// -------------------------------------------------------------------------------
EditorUI::WindowState& EditorUI::Context::GetOrCreateWindowState(Id _id)
{
	auto it = m_WindowStates.find(_id);
	if (it != m_WindowStates.end())
	{
		return it->second;	// 既存のものがあればそれを返す
	}

	// emplaceはキーと値をその場で構築して挿入する操作
	// 戻り値は　pair<iterator, bool>で、構造化束縛で受け取っている
	// insertedIt : 挿入された要素へのiterator, inserted : 実際に挿入されたか
	auto [insertedIt, inserted] = m_WindowStates.emplace(_id, WindowState{});
	insertedIt->second.WindowId = _id;
	m_WindowOrder.push_back(_id);	// 新規ウィンドウは背面から開始
	return insertedIt->second;
}

// -------------------------------------------------------------------------------
// 指定したウィンドウのIdを、z-order配列の末尾に移動する
// -------------------------------------------------------------------------------
void EditorUI::Context::BringToFront(Id _windowId)
{
	auto it = std::find(m_WindowOrder.begin(), m_WindowOrder.end(), _windowId);

	// it != end() : 見つかった場合のみ処理する
	if (it != m_WindowOrder.end() && std::next(it) != m_WindowOrder.end())
	{
		m_WindowOrder.erase(it);			// 元の位置から取り除く
		m_WindowOrder.push_back(_windowId);	// 末尾につけ直す
	}
}

// -------------------------------------------------------------------------------
// タイトルバーへのマウス操作を見て、ドラッグ開始・ドラッグ中の位置更新を行う
// -------------------------------------------------------------------------------
void EditorUI::Context::HandleTitleBarDrag(WindowFrame& _frame, WindowState& _state, const Rect2D& _titleBarRect)
{
	// 今フレームで押された瞬間の判定
	// 今フレームで押されている(m_Input.MouseDown[])場合だけtrueになる
	bool hovered		= _titleBarRect.Contains(m_Input.MousePos);
	bool justPressed	= m_Input.MouseDown[static_cast<int>(MouseButton::Mouse_Left)] && 
		!m_PrevInput.MouseDown[static_cast<int>(MouseButton::Mouse_Left)];

	// タイトルバーの上でクリックされた瞬間、かつまだ誰もドラッグされていない場合にドラッグ開始
	// m_DraggedWindow == 0のチェックがないと、重なったウィンドウを同時にドラッグしてしまう。
	if (hovered && justPressed && m_DraggedWindow == 0 && m_ResizeWindow == 0)
	{
		m_DraggedWindow = _state.WindowId;
		// マウス位置とウィンドウの左上のずれを記録
		// これがないと、ドラッグ開始時にウィンドウの左上が急にマウス位置へワープしてしまう
		m_DragOffset = { m_Input.MousePos.x - _state.Position.x, m_Input.MousePos.y - _state.Position.y };
		m_DragStartMousePos = m_Input.MousePos;	// ドラッグ判定の基準点として保存
		m_FocusedWindow = _state.WindowId;
		BringToFront(_state.WindowId);		// クリックしたウィンドウを最前面に持ってくる

		// 既にドッキング済みの場合、つかんだ瞬間に一旦フローティングに戻す
		if (m_DockSpaceInitialized)
		{
			m_DockSpace.UndockWindow(_state.WindowId);
		}
	}

	// 自分がドラッグ対象で、まだマウスが押され続けている間、位置を更新する
	if (m_DraggedWindow == _state.WindowId && m_Input.MouseDown[0])
	{
		_state.Position.x = m_Input.MousePos.x - m_DragOffset.x;
		_state.Position.y = m_Input.MousePos.y - m_DragOffset.y;

		if (m_DockSpaceInitialized)
		{
			DrawDockPreview(_frame);
		}
	}
}

// -------------------------------------------------------------------------------
// ホイール入力の反映
// -------------------------------------------------------------------------------
void EditorUI::Context::HandleScrollInput(WindowState& _state, const Rect2D& _windowRect)
{
	if (_state.MaxScrollY <= 0.0f) 
	{ return; }	// 前フレーム時点でスクロールの余地がなければ何もしない

	bool hovered = _windowRect.Contains(m_Input.MousePos);
	if (hovered && m_Input.MouseWheel != 0.0f)
	{
		constexpr float kScrollSpeed = 4.0f;	// ホイール１刻み当たりの移動量
		_state.Scroll.y -= m_Input.MouseWheel * kScrollSpeed;
		_state.Scroll.y = std::clamp(_state.Scroll.y, 0.0f, _state.MaxScrollY);
	}
}

// -------------------------------------------------------------------------------
// スクロールバーの描画とサムのドラッグ操作
// -------------------------------------------------------------------------------
void EditorUI::Context::DrawScrollbar(WindowFrame& _frame, WindowState& _state)
{
	const float titleBarHeight	= HasFlag(_frame.Flags, WindowFlags::NoTitleBar) ? 0.0f : m_Style.TitleBarHeight;
	const float trackTop		= _state.Position.y + titleBarHeight;
	const float trackHeight		= _state.Size.y - titleBarHeight;
	const float trackX			= _state.Position.x + _state.Size.x - m_Style.ScrollbarWidth;

	Rect2D track = MakeRect({ trackX, trackTop }, { m_Style.ScrollbarWidth, trackHeight });
	_frame.Draw.AddRectFilled(track, m_Style.ColorScrollbarBg);

	// サムの高さ = 見えている範囲 / コンテンツの全体の比率。位置はスクロール量の比率
	const float visibleHeight	= trackHeight;
	const float thumbRatio		= _frame.ContentHeight > 0.0f
		? (std::min)(1.0f, visibleHeight / _frame.ContentHeight) : 1.0f;
	const float thumbHeight		= (std::max)(20.0f, trackHeight * thumbRatio);	// 最小20はつかみやすさ
	const float scrollRatio		= _state.MaxScrollY > 0.0f ? (_state.Scroll.y / _state.MaxScrollY) : 0.0f;
	const float thumbY			= trackTop + (trackHeight - thumbHeight) * scrollRatio;

	Rect2D thumb = MakeRect({ trackX, thumbY }, { m_Style.ScrollbarWidth, thumbHeight });

	bool hovered = thumb.Contains(m_Input.MousePos);
	_frame.Draw.AddRectFilled(thumb, hovered ? m_Style.ColorScrollbarThumbHovered : m_Style.ColorScrollbarThumb);

	// サムを直接ドロップしてスクロールできるようにする
	bool justPressed = m_Input.MouseDown[static_cast<int>(MouseButton::Mouse_Left)]
		&& !m_PrevInput.MouseDown[static_cast<int>(MouseButton::Mouse_Left)];
	if (hovered && justPressed && m_DraggedScrollbarWindow == 0)
	{
		m_DraggedScrollbarWindow = _state.WindowId;
	}

	if (m_DraggedScrollbarWindow == _state.WindowId && m_Input.MouseDown[static_cast<int>(MouseButton::Mouse_Left)])
	{
		const float trackRange = trackHeight - thumbHeight;
		if (trackRange > 0.0f)
		{
			// マウスのY座標をサムの中心基準でトラック内の比率に変換し、そのままScrollへ反映する
			const float newRatio = std::clamp(
				(m_Input.MousePos.y - trackTop - thumbHeight * 0.5f) / trackRange, 0.0f, 1.0f);
			_state.Scroll.y = newRatio * _state.MaxScrollY;
		}
	}
}

// -------------------------------------------------------------------------------
// リサイズグリップの描画とつかみ操作
// -------------------------------------------------------------------------------
void EditorUI::Context::HandleResizeDrag(WindowState& _state, WindowFrame& _frame)
{
	const float gripSize	= m_Style.ResizeGripSize;
	Rect2D gripRect			= MakeRect(
		{ _state.Position.x + _state.Size.x - gripSize, _state.Position.y + _state.Size.y - gripSize },
		{ gripSize, gripSize });

	bool hovered		= gripRect.Contains(m_Input.MousePos);
	bool justPressed	= m_Input.MouseDown[static_cast<int>(MouseButton::Mouse_Left)] &&
		!m_PrevInput.MouseDown[static_cast<int>(MouseButton::Mouse_Left)];

	// タイトルバードラッグやスクロールバードラッグと競合しないよう
	// 他の操作が何も行われていないときだけリサイズを開始できるようにする
	if (hovered && justPressed && m_ResizeWindow == 0 && m_DraggedWindow == 0 && m_DraggedScrollbarWindow == 0)
	{
		m_ResizeWindow		= _state.WindowId;
		m_ResizeStartMouse	= m_Input.MousePos;
		m_ResizeStartSize	= _state.Size;
	}

	if (m_ResizeWindow == _state.WindowId && m_Input.MouseDown[static_cast<int>(MouseButton::Mouse_Left)])
	{
		const DirectX::XMFLOAT2 delta
		{
			m_Input.MousePos.x - m_ResizeStartMouse.x,
			m_Input.MousePos.y - m_ResizeStartMouse.y
		};

		// ウィンドウが潰れて操作不能にならないように最小サイズを設ける
		constexpr float kMinWidth = 100.0f;
		constexpr float kMinHeight = 80.0f;
		_state.Size.x = (std::max)(kMinWidth, m_ResizeStartSize.x + delta.x);
		_state.Size.y = (std::max)(kMinHeight, m_ResizeStartSize.y + delta.y);
	}

	_frame.Draw.AddRectFilled(gripRect, hovered ? m_Style.ColorButtonHovered : m_Style.ColorBorder);
}
