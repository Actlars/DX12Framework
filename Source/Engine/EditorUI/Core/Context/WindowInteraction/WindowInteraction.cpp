#include "WindowInteraction.h"

// -------------------------------------------------------------------------------
// 現在操作中か判定
// -------------------------------------------------------------------------------
bool EditorUI::WindowInteraction::IsBusy() const
{
	return m_Operation != EditorUI::WindowPointOperation::None;
}

// -------------------------------------------------------------------------------
// 現在実行中のWindow操作を取得
// -------------------------------------------------------------------------------
EditorUI::WindowPointOperation EditorUI::WindowInteraction::GetOperation() const
{
	return m_Operation;
}

// -------------------------------------------------------------------------------
// 現在移動操作中のWindowIdを取得
// -------------------------------------------------------------------------------
EditorUI::Id EditorUI::WindowInteraction::GetMovingWindow() const
{
	return m_Operation == EditorUI::WindowPointOperation::Move ? m_OperationWindow : 0;
}

// -------------------------------------------------------------------------------
// Window移動を開始した地点でのマウス座標を取得
// -------------------------------------------------------------------------------
const DirectX::XMFLOAT2& EditorUI::WindowInteraction::GetMoveStartMousePos() const
{
	return m_OperationStartMouse;
}

// -------------------------------------------------------------------------------
// 左マウスボタンが離された場合にWindow操作を終了
// -------------------------------------------------------------------------------
void EditorUI::WindowInteraction::ReleasePointerIfMouseUp(const EditorUI::InputTracker& _input)
{
	if (!_input.IsMouseDown(EditorUI::MouseButton::Mouse_Left))
	{
		ResetOperation();
	}
}

// -------------------------------------------------------------------------------
// 指定Windowに対する操作をキャンセル
// -------------------------------------------------------------------------------
void EditorUI::WindowInteraction::CancelForWindow(EditorUI::Id _windowId)
{
	if (m_OperationWindow == _windowId)
	{
		ResetOperation();
	}
}

// -------------------------------------------------------------------------------
// タイトルバーのドラッグによるWindow移動処理
// -------------------------------------------------------------------------------
EditorUI::WindowMoveResult EditorUI::WindowInteraction::HandleTitleBarDrag(
	EditorUI::WindowState&			_state,
	const EditorUI::Rect2D&			_titleBarRect, 
	const EditorUI::InputTracker&	_input,
	bool							_allowCapture)
{
	// Window移動処理の結果
	EditorUI::WindowMoveResult result;

	// 現在マウスカーソルがタイトルバー上にあるか取得
	const bool hovered = _titleBarRect.Contains(_input.GetMousePos());
	// このフレーム上で左マウスボタンが押されたか取得
	const bool justPressed = _input.IsMouseClicked(EditorUI::MouseButton::Mouse_Left);

	// タイトルバー上で左クリックされ、Window操作を開始できる状態ならMove操作を開始
	if (hovered && justPressed && CanStart(EditorUI::WindowPointOperation::Move, _allowCapture))
	{
		// このWindowに対するMove操作を開始
		BeginOperation(EditorUI::WindowPointOperation::Move, _state.WindowId, _input.GetMousePos());
		// Window左上から見たマウス座標の差分を保存
		m_MoveGrabOffset =
		{
			_input.GetMousePos().x - _state.Position.x,
			_input.GetMousePos().y - _state.Position.y
		};
		// このフレームで移動操作が開始されたことを通知
		result.Started = true;
	}

	// 現在Move操作中で、その操作対象がこのWindowであり、左マウスボタンが押され続けている場合
	if (m_Operation == EditorUI::WindowPointOperation::Move &&
		m_OperationWindow == _state.WindowId &&
		_input.IsMouseDown(EditorUI::MouseButton::Mouse_Left))
	{
		// 現在のマウス座標から操作開始時に保存した掴み位置のoffsetを引いてWindow左上座標を更新する
		_state.Position =
		{
			_input.GetMousePos().x - m_MoveGrabOffset.x,
			_input.GetMousePos().y - m_MoveGrabOffset.y
		};
		// 現在このWindowが操作中であることを保存
		result.Active = true;
	}
	return result;
}

// -------------------------------------------------------------------------------
// Dockから外したWindowをFloating状態にしてMove操作を開始
// -------------------------------------------------------------------------------
void EditorUI::WindowInteraction::BeginMoveFromDock(
	EditorUI::WindowState&		_state, 
	const EditorUI::Rect2D&		_previousDockBounds, 
	const DirectX::XMFLOAT2&	_pressedOffset, 
	const DirectX::XMFLOAT2&	_pressedMousePos, 
	const DirectX::XMFLOAT2&	_currentMousePos)
{
	// Undock後のFloatingWindowが大きくなりすぎないよう
	// 元のDock領域の70%を基準として最大サイズを決定する
	// 最小サイズで320px確保している
	const float maxFloatingWidth	= (std::max)(320.0f, _previousDockBounds.Width() * 0.70f);
	const float maxFloatingHeight	= (std::max)(320.0f, _previousDockBounds.Height() * 0.70f);

	// Undock後のWindow幅を調整する
	const float floatingWidth = _state.Size.x > 0.0f ? _state.Size.x : 420.0f;
	_state.Size.x = std::clamp(floatingWidth, 280.0f, maxFloatingWidth);

	// Undock後のWindow高さを調整する
	const float floatingHeight = _state.Size.y > 0.0f ? _state.Size.y : 300.0f;
	_state.Size.y = std::clamp(floatingHeight, 180.0f, maxFloatingHeight);

	// Dock上のタブを押した位置を基準として、このWindowのMove操作を開始する
	BeginOperation(WindowPointOperation::Move, _state.WindowId, _pressedMousePos);

	// Dock状態でマウスが掴んでいた位置をFloatingWindow内のつかみ位置として引き継ぐ
	m_MoveGrabOffset.x = std::clamp(_pressedOffset.x, 8.0f, (std::max)(8.0f, _state.Size.x - 8.0f));
	m_MoveGrabOffset.y = std::clamp(_pressedOffset.y, 2.0f, (std::max)(2.0f, _state.Size.y - 2.0f));

	// 現在のマウス座標から掴み位置を引くことで、Undockした瞬間もマウスが
	// 同じ座標を掴んだままになる用FloatingWindowの座標を決定
	_state.Position = { _currentMousePos.x - m_MoveGrabOffset.x, _currentMousePos.y - m_MoveGrabOffset.y };
}

// -------------------------------------------------------------------------------
// マウスホイールによるWindowの縦スクロール処理
// -------------------------------------------------------------------------------
void EditorUI::WindowInteraction::HandleScrollInput(
	EditorUI::WindowState&			_state,
	const EditorUI::Rect2D&			_windowRect, 
	const EditorUI::InputTracker&	_input) const
{
	// スクロール可能な範囲が存在しない
	if (_state.MaxScrollY <= 0.0f || !_windowRect.Contains(_input.GetMousePos())) 
	{ return; }

	// マウスホイール入力がない
	if (_input.GetMouseWheel() == 0.0f) 
	{ return; }

	// ホイール1段当たりのスクロール量
	constexpr float kScrollSpeed = 4.0f;
	// マウスホイール入力に応じて縦スクロール位置を更新
	_state.Scroll.y -= _input.GetMouseWheel() * kScrollSpeed;
	// スクロール位置が有効範囲を超えないように制限する
	_state.Scroll.y = std::clamp(_state.Scroll.y, 0.0f, _state.MaxScrollY);
}

// -------------------------------------------------------------------------------
// 縦スクロールバーの描画・ドラッグ操作
// -------------------------------------------------------------------------------
void EditorUI::WindowInteraction::DrawScrollbar(
	EditorUI::WindowFrame&			_frame,
	EditorUI::WindowState&			_state,
	const EditorUI::InputTracker&	_input,
	const EditorUI::Style&			_style,
	bool							_allowCapture)
{
	// Window内のコンテンツ描画領域を取得
	const EditorUI::Rect2D& contentRect = _frame.ContentRect;
	// スクロールバー全体の高さ
	const float		trackHeight = contentRect.Height();
	// スクロールバーをWindow右端に配置するためのX座標
	const float		trackX = _frame.WindowRect.Max.x - _style.ScrollbarWidth;

	if (trackHeight <= 0.0f)
	{
		return;
	}

	// スクロールバーのtrack領域を作成
	const EditorUI::Rect2D track = MakeRect({ trackX, contentRect.Min.y }, { _style.ScrollbarWidth,trackHeight });

	// スクロールバー背景を描画
	_frame.Draw.AddRectFilled(track, _style.ColorScrollbarBg);

	// 実際に表示可能なコンテンツ領域の高さを計算
	// WindowPadding分を上下から除外する
	const float visibleHeight = (std::max)(1.0f, trackHeight - _style.WindowPadding * 2.0f);
	// コンテンツ全体に対して、現在画面に見えている割合を計算する
	const float thumbRatio = _frame.ContentHeight > 0.0f
		? (std::min)(1.0f, visibleHeight / _frame.ContentHeight) : 1.0f;

	// thumbの高さを計算(最低20pxを保持する)
	const float thumbHeight = (std::max)(20.0f, trackHeight * thumbRatio);
	// 現在のスクロール位置を0.0～1.0へ変換
	// ScrollY = 0 → 0.0 : 最上部、ScrollY = MaxScrollY → 1.0 : 最下部
	const float scrollRatio = _state.MaxScrollY > 0.0f
		? _state.Scroll.y / _state.MaxScrollY : 0.0f;
	// スクロール割合に応じてthumbのY座標を決定
	// trackHeight - thumbHeight がthumb自身が移動できる範囲になる
	const float thumbY = contentRect.Min.y + (trackHeight - thumbHeight) * scrollRatio;

	// thumbの矩形を作成
	const EditorUI::Rect2D thumb = MakeRect({ trackX,thumbY }, { _style.ScrollbarWidth,thumbHeight });

	// マウスカーソルがthumbにあるか判定
	const bool hovered = thumb.Contains(_input.GetMousePos());
	// Hover状態によってthumbの色を変更して描画
	_frame.Draw.AddRectFilled(thumb, hovered ? _style.ColorScrollbarThumbHovered : _style.ColorScrollbarThumb);

	// thumb上で左クリックされ、Pointer取得が可能ならScrollbar操作を開始
	if (hovered && _input.IsMouseClicked(EditorUI::MouseButton::Mouse_Left) &&
		CanStart(EditorUI::WindowPointOperation::Scrollbar, _allowCapture))
	{
		BeginOperation(EditorUI::WindowPointOperation::Scrollbar, _state.WindowId, _input.GetMousePos());
	}

	// 現在このWindowのScrollbarをドラッグ中なら、マウス座標からスクロール座標を更新する
	if (m_Operation == EditorUI::WindowPointOperation::Scrollbar &&
		m_OperationWindow == _state.WindowId &&
		_input.IsMouseDown(EditorUI::MouseButton::Mouse_Left))
	{
		// thumbがtrack上を移動できる距離
		const float trackRange = trackHeight - thumbHeight;
		if (trackRange > 0.0f)
		{
			// マウス座標をtrack上の0.0～1.0へ変換
			// thumbの中心がマウス座標へ来るようにthumbHeight * 0.5fを引いている
			const float ratio = std::clamp(
				(_input.GetMousePos().y - contentRect.Min.y - thumbHeight * 0.5f) / trackRange,
				0.0f, 1.0f);
			// 0.0～1.0の割合を実際のスクロール量へ変換
			_state.Scroll.y = ratio * _state.MaxScrollY;
		}
	}
}

// -------------------------------------------------------------------------------
// Window右下のリサイズグリップ描画・リサイズ操作
// -------------------------------------------------------------------------------
void EditorUI::WindowInteraction::DrawResizeGrip(
	EditorUI::WindowState&			_state, 
	EditorUI::WindowFrame&			_frame, 
	const EditorUI::InputTracker&	_input,
	const EditorUI::Style&			_style,
	bool							_allowCapture)
{
	// リサイズグリップの大きさを取得
	const float gripSize = _style.ResizeGripSize;
	// Window右下にリサイズ操作用の領域を作成
	const EditorUI::Rect2D gripRect = MakeRect(
		{_frame.WindowRect.Max.x - gripSize, _frame.WindowRect.Max.y - gripSize},
		{ gripSize,gripSize });

	// マウスカーソルがリサイズグリップ上にあるか判定
	const bool hovered = gripRect.Contains(_input.GetMousePos());

	// グリップ上で左クリックされ、新しいWindow操作を開始できる場合はResize操作を開始する
	if (hovered && _input.IsMouseClicked(EditorUI::MouseButton::Mouse_Left) &&
		CanStart(EditorUI::WindowPointOperation::Resize, _allowCapture))
	{
		// このWindowに対するリサイズ操作を開始
		BeginOperation(EditorUI::WindowPointOperation::Resize, _state.WindowId, _input.GetMousePos());
		// リサイズ開始時点でのWindowサイズを保存
		m_ResizeStartSize = _state.Size;
	}

	// 現在このWindowをリサイズ中で、左マウスボタンが押され続けている場合
	if (m_Operation == EditorUI::WindowPointOperation::Resize &&
		m_OperationWindow == _state.WindowId &&
		_input.IsMouseDown(EditorUI::MouseButton::Mouse_Left))
	{
		// リサイズ開始位置から現在のマウス位置までの移動量を取得
		const DirectX::XMFLOAT2 delta
		{
			_input.GetMousePos().x - m_OperationStartMouse.x,
			_input.GetMousePos().y - m_OperationStartMouse.y
		};

		// Windowが小さくなりすぎないように最低サイズを設定
		constexpr float kMinWidth	= 100.0f;
		constexpr float kMinHeight	= 80.0f;
		// 開始時のWindowサイズにマウス移動量を加算して現在のWindowサイズを更新する
		_state.Size.x = (std::max)(kMinWidth, m_ResizeStartSize.x + delta.x);
		_state.Size.y = (std::max)(kMinHeight, m_ResizeStartSize.y + delta.y);
	}
	// リサイズグリップを描画
	_frame.Draw.AddRectFilled(gripRect, hovered ? _style.ColorButtonHovered : _style.ColorBorder);
}

// -------------------------------------------------------------------------------
// Window操作を開始できるか判定
// -------------------------------------------------------------------------------
bool EditorUI::WindowInteraction::CanStart(EditorUI::WindowPointOperation _operation, bool _allowCapture) const
{
	(void)_operation;
	// Pointerの取得が許可されており、他のWindow操作を実行中でなければ新しい操作を開始できる
	return _allowCapture && m_Operation == EditorUI::WindowPointOperation::None;
}

// -------------------------------------------------------------------------------
// Window操作を開始
// -------------------------------------------------------------------------------
void EditorUI::WindowInteraction::BeginOperation(
	EditorUI::WindowPointOperation	_operation, 
	EditorUI::Id					_windowId, 
	const DirectX::XMFLOAT2&		_mousePos)
{
	// 開始する操作の種類を保存
	m_Operation				= _operation;
	// 操作対象となるWindowIdを保存
	m_OperationWindow		= _windowId;
	// 操作を開始した瞬間のマウス座標を保存
	m_OperationStartMouse	= _mousePos;
}

// -------------------------------------------------------------------------------
// Window操作状態をリセット
// -------------------------------------------------------------------------------
void EditorUI::WindowInteraction::ResetOperation()
{
	// 現在実行中の操作を解除
	m_Operation				= EditorUI::WindowPointOperation::None;
	// 操作対象Windowを解除
	m_OperationWindow		= 0;
	// 操作開始時のマウス座標をリセット
	m_OperationStartMouse	= {};
	// Window移動時のマウスとWindow位置の差分をリセット
	m_MoveGrabOffset		= {};
	// リサイズ開始時のWindowサイズをリセット
	m_ResizeStartSize		= {};
}
