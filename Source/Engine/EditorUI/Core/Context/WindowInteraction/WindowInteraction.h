#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Id.h>
#include <Engine/EditorUI/Core/InputTracker/InputTracker.h>
#include <Engine/EditorUI/Core/Style.h>
#include <Engine/EditorUI/Core/Window/WindowFrame.h>
#include <Engine/EditorUI/Core/Window/WindowState.h>

namespace EditorUI
{
	struct WindowMoveResult
	{
		bool Started	= false;
		bool Active		= false;
	};

	enum class WindowPointOperation : std::uint8_t
	{
		None,
		Move,
		Resize,
		Scrollbar
	};

	// -------------------------------------------------------------------------------
	// WindowInteraction class
	// 
	// 概要 : 
	//	FloatingWindowのPointer操作を1つのCapture状態として管理する
	// -------------------------------------------------------------------------------
	class WindowInteraction
	{
	public:

		bool IsBusy() const;
		WindowPointOperation GetOperation() const;

		Id GetMovingWindow() const;
		const DirectX::XMFLOAT2& GetMoveStartMousePos() const;

		void ReleasePointerIfMouseUp(const InputTracker& _input);
		void CancelForWindow(Id _windowId);

		WindowMoveResult HandleTitleBarDrag(
			WindowState&		_state,
			const Rect2D&		_titleBarRect,
			const InputTracker& _input,
			bool				_allowCapture);

		void BeginMoveFromDock(
			WindowState&				_state,
			const Rect2D&				_previousDockBounds,
			const DirectX::XMFLOAT2&	_pressedOffset,
			const DirectX::XMFLOAT2&	_pressedMousePos,
			const DirectX::XMFLOAT2&	_currentMousePos);

		void HandleScrollInput(
			WindowState&		_state,
			const Rect2D&		_windowRect,
			const InputTracker& _input) const;

		void DrawScrollbar(
			WindowFrame&		_frame,
			WindowState&		_state,
			const InputTracker& _input,
			const Style&		_style,
			bool				_allowCapture);

		// -------------------------------------------------------------------------------
		// @brief	右下のリサイズグリップを描き、ドラッグによる拡縮を処理する
		//
		// @return	true : グリップの上にいる、またはこのWindowをリサイズ中
		//			呼び出し側がマウスカーソルの形を切り替える判断に使う
		// -------------------------------------------------------------------------------
		bool DrawResizeGrip(
			WindowState&		_state,
			WindowFrame&		_frame,
			const InputTracker& _input,
			const Style&		_style,
			bool				_allowCapture);

	private:

		bool CanStart(WindowPointOperation _operation, bool _allowCapture) const;
		void BeginOperation(WindowPointOperation _operation, Id _windowId, const DirectX::XMFLOAT2& _mousePos);
		void ResetOperation();

		WindowPointOperation m_Operation = WindowPointOperation::None;
		Id m_OperationWindow = 0;
		DirectX::XMFLOAT2 m_OperationStartMouse{};
		DirectX::XMFLOAT2 m_MoveGrabOffset{};
		DirectX::XMFLOAT2 m_ResizeStartSize{};

	};
}