#include "DockController.h"
#include <Engine/EditorUI/Text/Font/Font.h>
#include <Engine/EditorUI/Text/TextLayout/TextLayout.h>
#include <Engine/Utility/StringUtil/StringUtil.h>

namespace
{
	// 外周ドッキングで新しいLeafが占める割合
	constexpr float kDockSplitRatio = 0.28f;
}


void EditorUI::DockController::Init(const EditorUI::Rect2D& _bounds)
{
	m_DockSpace.Init(_bounds);
	m_Initialized = true;
}

void EditorUI::DockController::UpdateLayout(const EditorUI::Rect2D& _bounds)
{
	if (m_Initialized)
	{
		m_DockSpace.UpdateLayout(_bounds);
	}
}

bool EditorUI::DockController::IsInitialized() const
{
	return m_Initialized;
}

void EditorUI::DockController::NewFrame(const EditorUI::InputTracker& _input)
{
	if (!_input.IsMouseDown(EditorUI::MouseButton::Mouse_Left))
	{
		m_PressedTabWindow	= 0;
		m_PressedTabLeaf	= -1;
	}
}

// -------------------------------------------------------------------------------
// 指定されたWindowについて、どこにドッキングされていて、現在表示中のタブなのか
// 描画範囲はどこかを取得する関数
// -------------------------------------------------------------------------------
EditorUI::DockWindowInfo EditorUI::DockController::GetWindowInfo(EditorUI::Id _windowId) const
{
	EditorUI::DockWindowInfo info;
	if (!m_Initialized) 
	{ return info; }

	// Windowの所属先のLeafのIdを取得
	const int leafId = m_DockSpace.FindLeafOwning(_windowId);
	if (leafId == -1)	// ドッキングされていなければLeafIdは-1なので抜ける
	{ return info; }

	// LeafIdのDockNode(Dock木の1ノード)を取得
	// Leafノードには、その領域のドッキングされているWindow一覧が格納されている
	const EditorUI::DockNode* leaf = m_DockSpace.GetNode(leafId);
	if (leaf == nullptr || leaf->Windows.empty())
	{ return info; }

	// leaf内のWindow一覧から指定されたWindowを探す
	auto windowIt = std::find(leaf->Windows.begin(), leaf->Windows.end(), _windowId);
	if (windowIt == leaf->Windows.end()) 
	{ return info; }

	// 指定されたWindowがLeaf内で何番目のタブなのかを取得
	const int myIndex		= static_cast<int>(std::distance(leaf->Windows.begin(), windowIt));
	// 現在Activeになっているタブのインデックスを取得
	const int activeIndex	= std::clamp(leaf->ActiveTabIndex, 0, static_cast<int>(leaf->Windows.size()) - 1);

	// WindowはDockSpace内のLeafに所属しているため、ドッキング状態
	info.IsDocked		= true;
	// 自分のタブ番号と現在のアクティブタブ番号が一致しているか確認
	info.IsActiveTab	= myIndex == activeIndex;
	// Windowが所属しているLeafノードのId
	info.LeafId			= leafId;
	// Leaf領域の位置・サイズをWindowの描画領域として設定
	info.Bounds			= leaf->Bounds;
	return info;
}

bool EditorUI::DockController::IsDocked(EditorUI::Id _windowId) const
{
	return GetWindowInfo(_windowId).IsDocked;
}

int EditorUI::DockController::FindLeafOwning(EditorUI::Id _windowId) const
{
	return m_Initialized ? m_DockSpace.FindLeafOwning(_windowId) : -1;
}

// -------------------------------------------------------------------------------
// Dock領域の境界線(Splitter)に対するHover・クリック・ドラッグ操作を管理する
// -------------------------------------------------------------------------------
void EditorUI::DockController::UpdateSplitters(
	const EditorUI::InputTracker&	_input,
	const EditorUI::Style&			_style,
	bool							_allowPointerCapture)
{
	// DockControllerが未初期化の場合はSplitterのHovered,Draggedを解除
	if (!m_Initialized)
	{
		m_HoveredSplit = -1;
		m_DraggedSplit = -1;
		return;
	}
	
	// 左マウスボタンが現在押されているかを取得
	const bool leftDown = _input.IsMouseDown(EditorUI::MouseButton::Mouse_Left);

	// 既にSplitterをドラッグ中の場合
	if (m_DraggedSplit != -1)
	{
		// 左クリックを押し続けている間は
		// 現在のマウス位置に合わせてSplitterを移動する
		if (leftDown)
		{
			m_DockSpace.DragSplit(m_DraggedSplit, _input.GetMousePos());
		}
		else
		{
			// 左クリックが離されたらドラッグ終了
			m_DraggedSplit = -1;
		}
		// ドラッグ中は新しいSplitterのHover判定などは行わない
		return;
	}

	// Window/Widget/Tabなど、Splitter以外のUI操作がPointerを所有している間は新しくSplitterを開始しない
	if (!_allowPointerCapture || m_PressedTabWindow != 0)
	{
		m_HoveredSplit = -1;
		return;
	}

	// 現在のマウス位置に存在するSplitterを探す
	// SplitterThicknessはSplitterをつかめる判定範囲として使用
	m_HoveredSplit = m_DockSpace.FindSplitAt(_input.GetMousePos(), _style.SplitterThickness);

	// Splitter上で左クリックされた場合、そのSplitterのドラッグを開始する
	if (m_HoveredSplit != -1 && _input.IsMouseClicked(EditorUI::MouseButton::Mouse_Left))
	{
		m_DraggedSplit = m_HoveredSplit;
	}
}

bool EditorUI::DockController::IsSplitterHovered() const
{
	return m_HoveredSplit != -1;
}

bool EditorUI::DockController::IsPointerBusy() const
{
	return m_DraggedSplit != -1 || m_PressedTabWindow != 0;
}

// -------------------------------------------------------------------------------
// いまマウスを離したらどこへドッキングするかを求める
//
// 画面の内側ならマウス直下のLeafに対する判定、
// 画面の外へ出ているなら「出た方向の外周」に対する判定を返す
// プレビュー描画と実際のドッキングでこの関数を共有することで、
// 「プレビューは出ているのにドッキングされない」というズレを防ぐ
// -------------------------------------------------------------------------------
EditorUI::DockDropTarget EditorUI::DockController::ComputeDropTarget(
	const DirectX::XMFLOAT2&	_mousePos,
	const EditorUI::Style&		_style) const
{
	EditorUI::DockDropTarget target;

	if (!m_Initialized)
	{ return target; }

	// -------------------------------------------------------------------------------
	// 1. 画面外へ運ばれている場合は、出た辺への外周ドッキングを優先する
	// -------------------------------------------------------------------------------
	const EditorUI::DockSplitDir edge =
		ComputeScreenEdgeZone(_mousePos, _style.ScreenEdgeDockMargin);

	if (edge != EditorUI::DockSplitDir::None)
	{
		target.IsValid		= true;
		target.IsScreenEdge	= true;
		target.Direction	= edge;
		target.PreviewRect	= ComputeSplitPreviewRect(m_DockSpace.GetRootBounds(), edge, kDockSplitRatio);
		return target;
	}

	// -------------------------------------------------------------------------------
	// 2. 画面内ならマウス直下のLeafに対して判定する
	// -------------------------------------------------------------------------------
	const int leafId = m_DockSpace.FindLeafAt(_mousePos);
	if (leafId == -1)
	{ return target; }

	const EditorUI::DockNode* leaf = m_DockSpace.GetNode(leafId);
	if (leaf == nullptr)
	{ return target; }

	const EditorUI::DockSplitDir direction = ComputeDropZone(leaf->Bounds, _mousePos);
	if (direction == EditorUI::DockSplitDir::None)
	{ return target; }

	// DockSpace全体が空のRootLeaf一つだけの場合、Centerは特殊ケースとして扱わない
	if (m_DockSpace.IsSingleEmptyRoot() && direction == EditorUI::DockSplitDir::Center)
	{ return target; }

	target.IsValid		= true;
	target.LeafId		= leafId;
	target.Direction	= direction;

	// 空のLeafは分割せずそのまま埋まるため、Leaf全体がプレビュー範囲になる
	const bool fillsWholeLeaf =
		(direction == EditorUI::DockSplitDir::Center) || !m_DockSpace.IsLeafOccupied(leafId);

	target.PreviewRect = fillsWholeLeaf
		? leaf->Bounds
		: ComputeSplitPreviewRect(leaf->Bounds, direction, kDockSplitRatio);

	return target;
}

// -------------------------------------------------------------------------------
// ドラッグ中のWindowを、現在のマウス位置に応じてDockSpaceへドッキングできるか試す
// -------------------------------------------------------------------------------
bool EditorUI::DockController::TryDockWindow(
	EditorUI::Id				_windowId, 
	const DirectX::XMFLOAT2&	_mousePos, 
	const DirectX::XMFLOAT2&	_dragStartMousePos,
	const EditorUI::Style&		_style)
{
	if (!m_Initialized || _windowId == 0) 
	{ return false; }

	// 少しクリックしただけでドッキング扱いにならないように
	// ドラッグ開始位置から一定距離以上移動しているか確認
	constexpr float kDragThreshold = 5.0f;
	const float dx = _mousePos.x - _dragStartMousePos.x;
	const float dy = _mousePos.y - _dragStartMousePos.y;
	// 距離の平方を使ってドラッグ量を判定
	// sqrtを使わずに比較することで余計な計算を避ける
	if (dx * dx + dy * dy < kDragThreshold * kDragThreshold) 
	{ return false; }

	// プレビューとまったく同じ判定を使う
	const EditorUI::DockDropTarget target = ComputeDropTarget(_mousePos, _style);
	if (!target.IsValid)
	{ return false; }

	// -------------------------------------------------------------------------------
	// 画面外へ運ばれた場合 : 画面全体を割って外周へ吸着させる
	// -------------------------------------------------------------------------------
	if (target.IsScreenEdge)
	{
		const int newLeafId = m_DockSpace.SplitRoot(target.Direction, kDockSplitRatio);
		if (newLeafId == -1)
		{ return false; }

		m_DockSpace.DockWindowIntoLeaf(newLeafId, _windowId);
		return true;
	}

	// -------------------------------------------------------------------------------
	// 画面内 : マウス直下のLeafに対して処理する
	// -------------------------------------------------------------------------------

	// ドロップ先のLeafが空いている場合
	if (!m_DockSpace.IsLeafOccupied(target.LeafId))
	{
		// DockSpace全体が空のRootLeafひとつだけの状態の場合
		if (m_DockSpace.IsSingleEmptyRoot())
		{
			// 指定された方向にRootLeafを分割する
			const int newLeafId = m_DockSpace.SplitLeaf(target.LeafId, target.Direction, kDockSplitRatio);
			// 分割に成功した場合、新しく作られたLeafにWindowをドッキングする
			if (newLeafId != -1)
			{
				m_DockSpace.DockWindowIntoLeaf(newLeafId, _windowId);
				return true;
			}

			// Leafの分割に失敗
			return false;
		}

		// 通常の空のLeafの場合は分割せず、そのLeafへ直接Windowを配置する
		m_DockSpace.DockWindowIntoLeaf(target.LeafId, _windowId);
		return true;
	}

	// ドロップ先のLeafにすでにWindowがあり、Centerへドロップされた場合
	if (target.Direction == EditorUI::DockSplitDir::Center)
	{
		// Leafを分割せず、既存Windowと同じLeafへタブとして追加する
		m_DockSpace.DockWindowIntoLeaf(target.LeafId, _windowId);
		return true;
	}

	// Left/Right/Top/Bottomへドロップされた場合は、既存Leafを指定方向に分割する
	const int newLeafId = m_DockSpace.SplitLeaf(target.LeafId, target.Direction, kDockSplitRatio);
	if (newLeafId == -1) 
	{ return false; }

	// 分割によって新しく作成されたLeafへWindowを配置する
	m_DockSpace.DockWindowIntoLeaf(newLeafId, _windowId);
	return true;
}

// -------------------------------------------------------------------------------
// WindowをDockSpaceから取り外し、そのWindowやDockTreeに紐づいていた操作状態もリセットする
// -------------------------------------------------------------------------------
void EditorUI::DockController::RemoveWindow(EditorUI::Id _windowId)
{
	if (!m_Initialized || _windowId == 0)
	{ return; }

	// 指定されたWindowを、現在所属しているLeafから外す
	m_DockSpace.UndockWindow(_windowId);

	// 取り外したWindowが現在押下中のTabだった場合は、Tabの操作中の状態を解除する
	if (m_PressedTabWindow == _windowId)
	{
		m_PressedTabWindow	= 0;
		m_PressedTabLeaf	= -1;
	}

	// UndockによってSplittreeが縮約されえるため、NodeIdcacheを破棄する
	m_HoveredSplit = -1;
	m_DraggedSplit = -1;
}

// -------------------------------------------------------------------------------
// 既定レイアウトの構築用API
//
// DockSpaceへの薄い委譲にとどめ、判断はすべて呼び出し側(EditorApp)に任せる
// -------------------------------------------------------------------------------
bool EditorUI::DockController::IsEmpty() const
{
	return !m_Initialized || m_DockSpace.IsSingleEmptyRoot();
}

int EditorUI::DockController::SplitScreen(EditorUI::DockSplitDir _dir, float _ratio)
{
	return m_Initialized ? m_DockSpace.SplitRoot(_dir, _ratio) : -1;
}

int EditorUI::DockController::FindLeafAt(const DirectX::XMFLOAT2& _point) const
{
	return m_Initialized ? m_DockSpace.FindLeafAt(_point) : -1;
}

void EditorUI::DockController::DockWindowIntoLeaf(int _leafId, EditorUI::Id _windowId)
{
	if (m_Initialized && _leafId != -1 && _windowId != 0)
	{
		m_DockSpace.DockWindowIntoLeaf(_leafId, _windowId);
	}
}

EditorUI::Rect2D EditorUI::DockController::GetDockAreaBounds() const
{
	return m_Initialized ? m_DockSpace.GetRootBounds() : Rect2D{};
}

int EditorUI::DockController::GetRootLeafId() const
{
	return m_Initialized ? m_DockSpace.GetRootId() : -1;
}

// -------------------------------------------------------------------------------
// Leafに所属するWindowをタブとして描画し、
// クリックによるタブ切り替え、ドラッグによるUndock、×によるクローズを処理する
// -------------------------------------------------------------------------------
EditorUI::DockTabInteractionResult EditorUI::DockController::DrawTabBar(
	EditorUI::WindowFrame&			_frame,
	int								_leafId,
	const EditorUI::Rect2D&			_windowRect,
	const EditorUI::InputTracker&	_input,
	const EditorUI::Style&			_style,
	bool							_allowUndock,
	EditorUI::Font*					_pFont,
	const std::function<std::string_view(EditorUI::Id)>& _titleOf)
{
	// タブ操作によって発生した結果を返す
	EditorUI::DockTabInteractionResult result;

	if (!m_Initialized)
	{ return result; }

	// 指定されたLeafノードを取得
	// Leafがない、またはWindowを持っていない場合はタブバーを表示しない
	const EditorUI::DockNode* leaf = m_DockSpace.GetNode(_leafId);
	if (leaf == nullptr || leaf->Windows.empty())
	{ return result; }

	// タブバーの高さは、Windowのタイトルバーの高さと同じものを使用
	const float tabHeight = _style.TitleBarHeight;
	// Window上端にタブバー領域を作成
	const EditorUI::Rect2D tabBarRect = MakeRect(_windowRect.Min, { _windowRect.Width(), tabHeight });
	// タブバー全体の背景を描画
	_frame.Draw.AddRectFilled(tabBarRect, _style.ColorTabBarBg);

	constexpr float kUndockThreshold = 6.0f;				// Undockに必要な最低ドラッグ距離
	float			tabX			 = _windowRect.Min.x;	// 最初のタブのX座標

	// -------------------------------------------------------------------------------
	// タブは名前が読めないと意味がないため、文字幅からタブ幅を決める
	// 幅はStyleのMin/Maxで挟み、長すぎる名前はクリップで切り詰める
	// -------------------------------------------------------------------------------
	const auto measureTabWidth = [&](EditorUI::Id _windowId) -> float
	{
		if (_pFont == nullptr)
		{ return _style.TabMinWidth; }

		const std::wstring	wide		= StringUtil::Utf8ToWide(_titleOf(_windowId));
		const float			textWidth	= EditorUI::TextLayout::MeasureWidth(*_pFont, wide);

		const float needed = textWidth + _style.TabPaddingX * 2.0f + _style.TabCloseButtonSize + 4.0f;
		return std::clamp(needed, _style.TabMinWidth, _style.TabMaxWidth);
	};

	// Leafに所属しているWindowをタブとして描画する
	for (std::size_t i = 0; i < leaf->Windows.size(); ++i)
	{
		// このタブに対応するWindowId
		const EditorUI::Id tabWindowId = leaf->Windows[i];

		// タブの描画領域を作成
		const float				tabWidth = measureTabWidth(tabWindowId);
		const EditorUI::Rect2D	tabRect  = MakeRect({ tabX, _windowRect.Min.y }, { tabWidth, tabHeight });

		// 現在表示中のアクティブタブかどうか
		const bool active	= static_cast<int>(i) == leaf->ActiveTabIndex;
		// マウスカーソルがこのタブ上にあるか
		const bool hovered	= tabRect.Contains(_input.GetMousePos());

		// タブの状態に応じて描画色を変化
		// Active > Hover > Normalの優先順位
		const EditorUI::Color32 color = active
			? _style.ColorTabActive
			: hovered ? _style.ColorTabHovered : _style.ColorTabBg;

		// タブ背景と枠線を描画
		_frame.Draw.AddRectFilled(tabRect, color);
		_frame.Draw.AddRectOutline(tabRect, _style.ColorBorder, _style.BorderThickness);

		// -------------------------------------------------------------------------------
		// ×ボタンの領域。タブ名はこの手前までしか描かない
		// -------------------------------------------------------------------------------
		const EditorUI::Rect2D closeRect = MakeRect(
			{
				tabRect.Max.x - _style.TabPaddingX - _style.TabCloseButtonSize,
				tabRect.Min.y + (tabHeight - _style.TabCloseButtonSize) * 0.5f
			},
			{ _style.TabCloseButtonSize, _style.TabCloseButtonSize });

		// -------------------------------------------------------------------------------
		// タブ名
		// -------------------------------------------------------------------------------
		if (_pFont != nullptr)
		{
			const std::wstring wide = StringUtil::Utf8ToWide(_titleOf(tabWindowId));

			const DirectX::XMFLOAT2 textPos
			{
				tabRect.Min.x + _style.TabPaddingX,
				tabRect.Min.y + (tabHeight - _pFont->GetLineHeight()) * 0.5f
			};

			// ラベルが×ボタンへ重ならないよう、テキスト用の領域でクリップする
			_frame.Draw.PushClipRect({ tabRect.Min, { closeRect.Min.x - 2.0f, tabRect.Max.y } });
			_frame.Draw.AddText(textPos, active ? _style.ColorTextBright : _style.ColorText, wide, *_pFont);
			_frame.Draw.PopClipRect();
		}

		// -------------------------------------------------------------------------------
		// ×ボタン
		// アクティブタブとホバー中のタブにだけ出す
		// 常に全タブへ表示すると、並んだときに情報量が多くなりすぎるため
		// -------------------------------------------------------------------------------
		const bool closeVisible = active || hovered;
		const bool closeHovered = closeVisible && closeRect.Contains(_input.GetMousePos());

		if (closeVisible)
		{
			if (closeHovered)
			{
				_frame.Draw.AddRectFilled(closeRect, _style.ColorButtonActive);
			}

			// 線を斜めに引く機能がないため、中央の小さな四角で「閉じる」を表す
			const float inset = closeRect.Width() * 0.28f;
			_frame.Draw.AddRectFilled(
				{
					{ closeRect.Min.x + inset, closeRect.Min.y + inset },
					{ closeRect.Max.x - inset, closeRect.Max.y - inset }
				},
				closeHovered ? _style.ColorTextBright : _style.ColorTextMuted);
		}

		// タブ上で左クリックされた場合
		if (hovered && _input.IsMouseClicked(EditorUI::MouseButton::Mouse_Left))
		{
			if (closeHovered)
			{
				// ×が押されたときはタブ切り替えもUndockも行わず、閉じる要求だけを返す
				result.ClosedWindow = tabWindowId;
			}
			else
			{
				// クリックされたタブをアクティブにする
				m_DockSpace.SetActiveTab(_leafId, static_cast<int>(i));
				// 押下中のタブ情報を保存。後続のドラッグ判定でUndockするために使用
				m_PressedTabWindow		= tabWindowId;
				m_PressedTabLeaf		= _leafId;
				// タブを押した瞬間のマウス座標
				m_PressedTabMousePos	= _input.GetMousePos();
				// Windowを左上からみた、マウスを押した瞬間の相対座標を保存
				m_PressedTabOffset		=
				{
					_input.GetMousePos().x - _windowRect.Min.x,
					_input.GetMousePos().y - _windowRect.Min.y
				};
				// このWindowへフォーカスを移すよう呼び出し元へ通知
				result.FocusWindow		= tabWindowId;
			}
		}

		// 次のタブの描画位置に移動
		tabX += tabWidth;
	}

	// 特定の状況ではUndockを行わない
	// 1. Undockが許可されていない
	// 2. 現在押下中のタブが存在しない
	// 3. 押下したタブが別Leafに所属している
	// 4. 左マウスボタンがすでにはなされている場合
	if (!_allowUndock || m_PressedTabWindow == 0 ||
		m_PressedTabLeaf != _leafId || !_input.IsMouseDown(EditorUI::MouseButton::Mouse_Left))
	{
		return result;
	}

	// タブを押した瞬間から現在位置までの移動量を計算
	const float dx = _input.GetMousePos().x - m_PressedTabMousePos.x;
	const float dy = _input.GetMousePos().y - m_PressedTabMousePos.y;
	if (dx * dx + dy * dy < kUndockThreshold * kUndockThreshold)
	{ return result; }

	// UndockするWindowIdを退避
	const Id windowId			= m_PressedTabWindow;
	// Undock後に必要になる情報を結果へ保存
	result.FocusWindow			= windowId;
	result.UndockedWindow		= windowId;
	// Undockすると、DockTreeが変化してLeafが無効になる可能性があるので、必要なLeaf情報はUndock前に保存する
	result.PreviousLeafBounds	= leaf->Bounds;
	// タブを押した瞬間のマウス位置
	result.PressedMousePos		= m_PressedTabMousePos;
	// Window内のどの位置を掴んでいたか
	result.PressedOffset		= m_PressedTabOffset;

	// Windowを現在のLeafから取り外す
	m_DockSpace.UndockWindow(windowId);
	// タブ押下状態の解除
	m_PressedTabWindow	= 0;
	m_PressedTabLeaf	= -1;
	return result;
}

// -------------------------------------------------------------------------------
// ドッキング先のプレビュー描画を行う
//
// 移動中ウィンドウのDrawListではなくオーバーレイへ描く
// ウィンドウ自身のDrawListはウィンドウ矩形でクリップされるため、
// 画面端や他ウィンドウ上のプレビューが表示されなくなってしまう
// -------------------------------------------------------------------------------
void EditorUI::DockController::DrawPreview(
	EditorUI::DrawList&			_overlay,
	const DirectX::XMFLOAT2&	_mousePos,
	const EditorUI::Style&		_style) const
{
	const EditorUI::DockDropTarget target = ComputeDropTarget(_mousePos, _style);
	if (!target.IsValid || !target.PreviewRect.IsValid())
	{ return; }

	// ドッキング予定領域を半透明で塗り、輪郭で範囲をはっきり示す
	_overlay.AddRectFilled(target.PreviewRect, _style.ColorDockPreview);
	_overlay.AddRectOutline(target.PreviewRect, _style.ColorDockPreviewBorder, 2.0f);
}

// -------------------------------------------------------------------------------
// マウス位置からドッキング方向を判定
// -------------------------------------------------------------------------------
EditorUI::DockSplitDir EditorUI::DockController::ComputeDropZone(const EditorUI::Rect2D& _bounds, const DirectX::XMFLOAT2& _mousePos) const
{
	// マウスが対象領域の外にある、または領域サイズが無効な場合はドロップ対象外
	if (!_bounds.Contains(_mousePos) || _bounds.Width() <= 0.0f || _bounds.Height() <= 0.0f)
	{ return EditorUI::DockSplitDir::None; }

	//マウス位置をBounds内の0.0から1.0の相対座標へ変換
	// 左端 : relX = 0.0
	// 右端 : relX = 1.0
	// 上端 : relY = 0.0
	// 下端 : relY = 1.0
	const float relX = (_mousePos.x - _bounds.Min.x) / _bounds.Width();
	const float relY = (_mousePos.y - _bounds.Min.y) / _bounds.Height();

	// 端から15%以内をLeft/Right/Top/Bottomのドロップ判定領域として使用
	constexpr float kEdgeZone			= 0.15f;
	// Center判定領域として使用
	constexpr float kCenterHalfExtent	= 0.11f;

	// Centerのドロップ領域内にマウスがあるか判定
	if (std::abs(relX - 0.5f) <= kCenterHalfExtent &&
		std::abs(relY - 0.5f) <= kCenterHalfExtent)
	{
		return EditorUI::DockSplitDir::Center;
	}

	// マウス位置から各辺までの距離を0.0から1.0で取得
	const float distLeft	= relX;
	const float distRight	= 1.0f - relX;
	const float distTop		= relY;
	const float distBottom	= 1.0f - relY;
	// 4辺のうち最も近い辺までの距離を取得
	const float minDistance = (std::min)({ distLeft,distRight,distTop,distBottom });

	// 最も近い辺でも15%以上離れている場合はどこにも属さない
	if (minDistance > kEdgeZone)
	{
		return EditorUI::DockSplitDir::None;
	}

	// 最も近い辺をドッキング方向として返す
	if (minDistance == distLeft)	{ return EditorUI::DockSplitDir::Left; }
	if (minDistance == distRight)	{ return EditorUI::DockSplitDir::Right; }
	if (minDistance == distTop)		{ return EditorUI::DockSplitDir::Top; }
	return EditorUI::DockSplitDir::Bottom;
}

// -------------------------------------------------------------------------------
// マウスがルート領域(=画面)の外へ出ているとき、どの辺の外側かを判定する
//
// ウィンドウを画面の外まで運んだときに、その方向の外周へドッキングさせるための判定
// 画面際での誤爆を避けるため、_marginだけ外に出て初めて反応させる
// 角へ出した場合は、より深く外へ出ている方の辺を採用する
// -------------------------------------------------------------------------------
EditorUI::DockSplitDir EditorUI::DockController::ComputeScreenEdgeZone(
	const DirectX::XMFLOAT2& _mousePos, float _margin) const
{
	const EditorUI::Rect2D root = m_DockSpace.GetRootBounds();
	if (root.Width() <= 0.0f || root.Height() <= 0.0f)
	{ return EditorUI::DockSplitDir::None; }

	// 各辺について「どれだけ外へ出ているか」を求める。内側なら0以下になる
	const float outLeft		= root.Min.x - _mousePos.x;
	const float outRight	= _mousePos.x - root.Max.x;
	const float outTop		= root.Min.y - _mousePos.y;
	const float outBottom	= _mousePos.y - root.Max.y;

	const float maxOut = (std::max)({ outLeft, outRight, outTop, outBottom });

	// どの辺からも出ていない、または出方が浅い場合は通常のドッキング判定に任せる
	if (maxOut < _margin)
	{ return EditorUI::DockSplitDir::None; }

	// いちばん深く外へ出ている辺を採用する
	if (maxOut == outLeft)	{ return EditorUI::DockSplitDir::Left; }
	if (maxOut == outRight)	{ return EditorUI::DockSplitDir::Right; }
	if (maxOut == outTop)	{ return EditorUI::DockSplitDir::Top; }
	return EditorUI::DockSplitDir::Bottom;
}

// -------------------------------------------------------------------------------
// 分割方向から、実際にウィンドウが収まる領域を求める
// プレビューと実際の配置がずれないよう、比率も分割時と同じ値を使う
// -------------------------------------------------------------------------------
EditorUI::Rect2D EditorUI::DockController::ComputeSplitPreviewRect(
	const EditorUI::Rect2D& _bounds, EditorUI::DockSplitDir _dir, float _ratio)
{
	EditorUI::Rect2D preview = _bounds;

	switch (_dir)
	{
	case EditorUI::DockSplitDir::Left:
		preview.Max.x = preview.Min.x + preview.Width() * _ratio;
		break;
	case EditorUI::DockSplitDir::Right:
		preview.Min.x = preview.Max.x - preview.Width() * _ratio;
		break;
	case EditorUI::DockSplitDir::Top:
		preview.Max.y = preview.Min.y + preview.Height() * _ratio;
		break;
	case EditorUI::DockSplitDir::Bottom:
		preview.Min.y = preview.Max.y - preview.Height() * _ratio;
		break;
	case EditorUI::DockSplitDir::Center:
	default:
		// Centerは領域全体がそのまま対象になる
		break;
	}

	return preview;
}
