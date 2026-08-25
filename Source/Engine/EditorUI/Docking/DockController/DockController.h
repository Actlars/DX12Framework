#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Id.h>
#include <Engine/EditorUI/Core/Input.h>
#include <Engine/EditorUI/Core/Types.h>
#include <Engine/EditorUI/Core/Style.h>
#include <Engine/EditorUI/Core/Window.h>
#include <Engine/EditorUI/Core/Context/WindowManager/WindowManager.h>
#include <Engine/EditorUI/Core/InputTracker/InputTracker.h>
#include <Engine/EditorUI/Docking/DockSpace/DockSpace.h>

namespace EditorUI
{
	class Font;

	struct DockWindowInfo
	{
		bool	IsDocked	= false;
		bool	IsActiveTab = false;
		int		LeafId		= -1;
		Rect2D	Bounds{};
	};

	struct DockTabInteractionResult
	{
		Id FocusWindow		= 0;	// フォーカスを移すべきウィンドウ
		Id UndockedWindow	= 0;	// ドラッグでドックから外れたウィンドウ
		Id ClosedWindow		= 0;	// タブの×が押されたウィンドウ
		Rect2D PreviousLeafBounds{};
		DirectX::XMFLOAT2 PressedMousePos{};
		DirectX::XMFLOAT2 PressedOffset{};
	};

	// -------------------------------------------------------------------------------
	// DockDropTarget struct
	//
	// 概要 :
	//	いま離したらどこへドッキングするか、という判定結果
	//	プレビュー描画と実際のドッキングで同じ判定を使うために構造体へまとめている
	// -------------------------------------------------------------------------------
	struct DockDropTarget
	{
		bool			IsValid		= false;
		bool			IsScreenEdge = false;	// 画面外へ運ばれたことによる外周ドッキングか
		int				LeafId		= -1;		// IsScreenEdgeがtrueのときは未使用
		DockSplitDir	Direction	= DockSplitDir::None;
		Rect2D			PreviewRect{};			// 実際にウィンドウが収まる領域
	};

	// -------------------------------------------------------------------------------
	// DockController class
	// 
	// 概要 : DockSpaceというDockツリー(Model)に対するユーザー操作を担当するController
	// -------------------------------------------------------------------------------
	class DockController
	{
	public:

		// -------------------------------------------------------------------------------
		// @brief	初期化
		// -------------------------------------------------------------------------------
		void Init(const Rect2D& _bounds);

		// -------------------------------------------------------------------------------
		// @brief	画面サイズが変わったときに、ドック領域の再計算を行う
		// -------------------------------------------------------------------------------
		void UpdateLayout(const Rect2D& _bounds);

		bool IsInitialized() const;

		// Mouseが離されたとき、Tabdrag候補を破棄する
		void NewFrame(const InputTracker& _input);

		DockWindowInfo GetWindowInfo(Id _windowId)	const;
		bool IsDocked(Id _windowId)					const;
		int FindLeafOwning(Id _windowId)			const;

		void UpdateSplitters(
			const InputTracker& _input, 
			const Style&		_style,
			bool				_allowPointerCapture);

		bool IsSplitterHovered()	const;
		bool IsPointerBusy()		const;

		// -------------------------------------------------------------------------------
		// @brief	ドラッグ中のウィンドウを、いまの位置に応じてドッキングさせる
		//
		//	マウスが画面の外にある場合は、その方向の外周へ吸着させる
		// -------------------------------------------------------------------------------
		bool TryDockWindow(
			Id _windowId,
			const DirectX::XMFLOAT2& _mousePos,
			const DirectX::XMFLOAT2& _dragStartMousePos,
			const Style& _style);

		void RemoveWindow(Id _windowId);

		// -------------------------------------------------------------------------------
		// 既定レイアウトの構築用API
		//
		//	アプリ起動時に「左にヒエラルキー、右にインスペクタ」といった
		//	初期配置を用意するために使う。通常の操作では呼ばない
		// -------------------------------------------------------------------------------

		// まだ1つもドッキングされていないか
		bool IsEmpty() const;

		// 画面全体を指定方向に分割し、新しいLeafのIdを返す
		int SplitScreen(DockSplitDir _dir, float _ratio);

		// 指定座標を含むLeafのIdを返す
		int FindLeafAt(const DirectX::XMFLOAT2& _point) const;

		// 指定のLeafへウィンドウを直接ドッキングさせる
		void DockWindowIntoLeaf(int _leafId, Id _windowId);

		// ドッキング領域全体の矩形
		Rect2D GetDockAreaBounds() const;

		// ルートノードのId。まだ分割されていない状態では、そのまま中央の区画を指す
		int GetRootLeafId() const;

		// -------------------------------------------------------------------------------
		// @brief	Leafに属するウィンドウをタブとして描画し、切り替え/Undock/クローズを処理する
		//
		// @param[in]	_titleOf	ウィンドウIdからタブに表示する名前を引くための関数
		//							DockControllerはウィンドウの名前を持たないため外部から渡す
		// -------------------------------------------------------------------------------
		DockTabInteractionResult DrawTabBar(
			WindowFrame&		_frame,
			int					_leafId,
			const Rect2D&		_windowRect,
			const InputTracker& _input,
			const Style&		_style,
			bool				_allowUndock,
			Font*				_pFont,
			const std::function<std::string_view(Id)>& _titleOf);

		// -------------------------------------------------------------------------------
		// @brief	いまマウスを離したらどこへドッキングするかを求める
		//
		//	画面(ルート領域)の外にマウトが出ている場合は、その方向の外周ドッキングを返す
		// -------------------------------------------------------------------------------
		DockDropTarget ComputeDropTarget(const DirectX::XMFLOAT2& _mousePos, const Style& _style) const;

		// -------------------------------------------------------------------------------
		// @brief	ドロップ先のプレビューを描く
		//
		//	移動中ウィンドウのDrawListではなくオーバーレイに描く
		//	ウィンドウ矩形でクリップされると、画面端のプレビューが見えなくなるため
		// -------------------------------------------------------------------------------
		void DrawPreview(DrawList& _overlay, const DirectX::XMFLOAT2& _mousePos, const Style& _style) const;

	private:

		DockSplitDir ComputeDropZone(const Rect2D& _bounds, const DirectX::XMFLOAT2& _mousePos) const;

		// マウスがルート領域の外にあるとき、どの辺の外側かを返す
		DockSplitDir ComputeScreenEdgeZone(const DirectX::XMFLOAT2& _mousePos, float _margin) const;

		// 分割方向とLeaf矩形から、実際にウィンドウが収まる領域を求める
		static Rect2D ComputeSplitPreviewRect(const Rect2D& _bounds, DockSplitDir _dir, float _ratio);

	private:

		DockSpace	m_DockSpace;
		bool		m_Initialized = false;

		Id	m_PressedTabWindow	= 0;
		int m_PressedTabLeaf	= -1;

		DirectX::XMFLOAT2 m_PressedTabMousePos{};
		DirectX::XMFLOAT2 m_PressedTabOffset{};

		int m_HoveredSplit = -1;
		int m_DraggedSplit = -1;

	};
}
