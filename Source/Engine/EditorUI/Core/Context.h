#pragma once

// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Types.h"
#include "Id.h"
#include "Style.h"
#include "Window.h"
#include "Input.h"

namespace EditorUI
{
	// Editor全体のフレーム管理・ウィンドウのzオーダー管理・入力状態の保持を行うクラス
	class Context
	{
	public:

		Context();
		~Context();

		void NewFrame(const InputState& _input);
		void EndFrame();

		bool BeginWindow(std::string_view _title, bool* _isOpen = nullptr, WindowFlags _flags = WindowFlags::None);
		void EndWindow();

		const Style& GetStyle() const { return m_Style; }
		void SetStyle(const Style& _style) { m_Style = _style; }

		IdStack& GetIdStack() { return m_IdStack; }
		WindowFrame* GetCurrentWindow() { return m_CurrentWindow; }

		// マウスがいずれかのUIウィンドウ上にあるかチェック
		bool IsMouseOverAnyWindow() const;

		// Render層が消費する最終出力。背面→前面の順でウィンドウごとにDrawListを並べる
		struct CompositedFrame
		{
			std::vector<const DrawList*> WindowDrawLists;
		};
		const CompositedFrame& GetCompositedFrame() const { return m_CompositedFrame; }

		// Widgets層がホバー/クリック判定に使う入力アクセサ
		const DirectX::XMFLOAT2& GetMousePos() const { return m_Input.MousePos; }
		bool IsMouseDown(int _button)		const { return m_Input.MouseDown[_button]; }
		bool IsMouseClicked(int _button)	const { return m_Input.MouseDown[_button] && !m_PrevInput.MouseDown[_button]; }
		bool IsMouseReleased(int _button)	const { return !m_Input.MouseDown[_button] && m_PrevInput.MouseDown[_button]; }

	private:

		WindowState& GetOrCreateWindowState(Id _id);
		void BringToFront(Id _windowId);
		void HandleTitleBarDrag(WindowState& _state, const Rect2D& _titleBarRect);

		Style m_Style;
		IdStack m_IdStack;

		std::unordered_map<Id, WindowState> m_WindowStates;
		std::vector<Id> m_WindowOrder;	// 背面（先頭）→前面（末尾）

		InputState m_Input;
		InputState m_PrevInput;

		Id m_FocusedWindow = 0;
		Id m_DraggedWindow = 0;
		DirectX::XMFLOAT2 m_DragOffset{};

		WindowFrame* m_CurrentWindow = nullptr;
		std::vector<std::unique_ptr<WindowFrame>> m_ActiveWindowFrame;	// このフレームでBeginされた分のみ生存

		CompositedFrame m_CompositedFrame;
	};
}
