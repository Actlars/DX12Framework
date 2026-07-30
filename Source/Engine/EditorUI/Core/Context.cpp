#include "Context.h"


EditorUI::Context::Context()
{
	// コンストラクタでスタイルの初期値をコピー
	m_Style = GetDefaultStyle();
}

EditorUI::Context::~Context()
{ /* DO_NOTHING */ }


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

	// マウスの左ボタンが離されていれば、ドラッグ中のウィンドウを強制的になしにする
	if (!m_Input.MouseDown[static_cast<int>(MouseButton::Mouse_Left)])
	{
		m_DraggedWindow = 0;
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

	bool hasTitleBar = !HasFlag(_flags, WindowFlags::NoTitleBar);		// NoTitleBarフラグが立っていなければタイトルバーを描く
	float titleBarHeight = hasTitleBar ? m_Style.TitleBarHeight : 0.0f;

	// -------------------------------------------------------------------------------
	// ウィンドウ全体のクリップ
	// -------------------------------------------------------------------------------
	// ウィンドウ全体(背景＋枠線)を描画コマンドに積む
	Rect2D windowRect = MakeRect(state.Position, state.Size);
	frame->Draw.PushClipRect(windowRect);

	// 背景
	frame->Draw.AddRectFilled(windowRect, m_Style.ColorWindowBg);

	// 枠線
	frame->Draw.AddRectOutline(windowRect, m_Style.ColorBorder, m_Style.BorderThickness);

	// タイトルバー
	if (hasTitleBar)
	{
		Rect2D titleBarRect = MakeRect(state.Position, { state.Size.x, titleBarHeight });
		bool focused = (m_FocusedWindow == id);	// 自分がフォーカス中のウィンドウかどうかで色を変える
		frame->Draw.AddRectFilled(titleBarRect, focused ? m_Style.ColorTitleBarBgFocused : m_Style.ColorTitleBarBg);

		// NoMoveフラグが立っていなければ、タイトルバーのドラッグ判定を行う
		if (!HasFlag(_flags, WindowFlags::NoMove))
		{
			HandleTitleBarDrag(state, titleBarRect);
		}
	}

	// コンテンツ領域（ウィジェットを実際に置いていく場所）の開始位置を計算する
	frame->ContentOrigin = { state.Position.x + m_Style.WindowPadding,
								state.Position.y + titleBarHeight + m_Style.WindowPadding };
	frame->CursorPos = frame->ContentOrigin;	// 最初のウィジェットはこの位置から置き始める

	// コンテンツ領域はウィンドウ矩形でクリップする。Widgets層はこの中でAddRect/AddTextを積む
	Rect2D contentClip = MakeRect(
		{ state.Position.x, state.Position.y + titleBarHeight },
		{ state.Size.x, state.Size.y - titleBarHeight });

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
	m_CurrentWindow->Draw.PopClipRect();	// BeginWindowでPushした分をここで戻す
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
void EditorUI::Context::HandleTitleBarDrag(WindowState& _state, const Rect2D& _titleBarRect)
{
	// 今フレームで押された瞬間の判定
	// 今フレームで押されている(m_Input.MouseDown[])場合だけtrueになる
	bool hovered = _titleBarRect.Contains(m_Input.MousePos);
	bool justPressed = m_Input.MouseDown[static_cast<int>(MouseButton::Mouse_Left)] && !m_PrevInput.MouseDown[0];

	// タイトルバーの上でクリックされた瞬間、かつまだ誰もドラッグされていない場合にドラッグ開始
	// m_DraggedWindow == 0のチェックがないと、重なったウィンドウを同時にドラッグしてしまう。
	if (hovered && justPressed && m_DraggedWindow == 0)
	{
		m_DraggedWindow = _state.WindowId;
		// マウス位置とウィンドウの左上のずれを記録
		// これがないと、ドラッグ開始時にウィンドウの左上が急にマウス位置へワープしてしまう
		m_DragOffset = { m_Input.MousePos.x - _state.Position.x, m_Input.MousePos.y - _state.Position.y };
		m_FocusedWindow = _state.WindowId;
		BringToFront(_state.WindowId);		// クリックしたウィンドウを最前面に持ってくる
	}

	// 自分がドラッグ対象で、まだマウスが押され続けている間、位置を更新する
	if (m_DraggedWindow == _state.WindowId && m_Input.MouseDown[0])
	{
		_state.Position.x = m_Input.MousePos.x - m_DragOffset.x;
		_state.Position.y = m_Input.MousePos.y - m_DragOffset.y;
	}
}
