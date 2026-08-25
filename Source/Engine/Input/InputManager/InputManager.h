#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/Input/KeyboardInput/KeyboardInput.h>
#include <Engine/Input/MouseInput/MouseInput.h>

namespace Input
{
	enum class MouseCaptureOwner
	{
		None,
		EditorUI,
		SceneCamera
	};

	class InputManager
	{
	public:

		bool Init(HWND _hWnd);
		void Term();

		// キーボード・マウスの状態を更新
		void Update();

		// 左クリックした位置に応じて入力先を決定
		void ResolveMouseCapture(bool _mouseOverEditorUI);

		// Wind32からポーリングできないメッセージを受け取る
		void ProcessWindowMessage(UINT _msg, WPARAM _wp, LPARAM _lp);

		void ReleaseMouseCapture();

		const KeyboardInput& GetKeyboardInput() const 
		{ return m_KeyboardInput; }

		const MouseInput& GetMouseInput() const 
		{ return m_MouseInput; }

		KeyboardInput& GetKeyboardInput() 
		{ return m_KeyboardInput; }

		MouseInput& GetMouseInput() 
		{ return m_MouseInput; }

		MouseCaptureOwner GetMouseCaptureOwner() const 
		{ return m_MouseCaptureOwner; }

		bool IsCameraControlActive() const 
		{ return m_MouseCaptureOwner == MouseCaptureOwner::SceneCamera; }

		bool IsEditorControlActive() const 
		{ return m_MouseCaptureOwner == MouseCaptureOwner::EditorUI; }

	private:

		HWND m_hWnd = nullptr;

		KeyboardInput m_KeyboardInput;
		MouseInput m_MouseInput;

		MouseCaptureOwner m_MouseCaptureOwner = MouseCaptureOwner::None;
	};
}
