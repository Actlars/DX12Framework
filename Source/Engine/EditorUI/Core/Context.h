#pragma once

// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Types.h"
#include "Id.h"
#include "Style.h"
#include "Window.h"
#include "Input.h"
#include <Engine/EditorUI/Docking/DockSpace/DockSpace.h>

namespace EditorUI
{
	// Editor全体のフレーム管理・ウィンドウのzオーダー管理・入力状態の保持を行うクラス
	class Context
	{
	public:

		Context();
		~Context();

		// -------------------------------------------------------------------------------
		// @brief	ドッキング機能を有効化する。
		// -------------------------------------------------------------------------------
		void InitDockSpace(const Rect2D& _screenBounds);

		// -------------------------------------------------------------------------------
		// @brief	画面サイズが変わったとき等に、ドック領域の再計算を行う
		// -------------------------------------------------------------------------------
		void UpdateDockSpaceLayout(const Rect2D& _screenBounds);

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

		DockSplitDir ComputeDropZone(const Rect2D& _leafBounds, const DirectX::XMFLOAT2& _mousePos) const;
		void HandleDockDrop(WindowState& _state);
		void DrawTabBar(WindowFrame& _frame, const DockNode& _leaf, int _leafId, const Rect2D& _windowRect);
		void DrawDockPreview(WindowFrame& _frame);

		WindowState& GetOrCreateWindowState(Id _id);
		void BringToFront(Id _windowId);
		void HandleTitleBarDrag(WindowFrame& _frame, WindowState& _state, const Rect2D& _titleBarRect);
		
		void HandleScrollInput(WindowState& _state, const Rect2D& _windowRect);
		void DrawScrollbar(WindowFrame& _frame, WindowState& _state);
		void HandleResizeDrag(WindowState& _state, WindowFrame& _frame);

		DockSpace	m_DockSpace;
		bool		m_DockSpaceInitialized = false;

		Style	m_Style;
		IdStack m_IdStack;

		std::unordered_map<Id, WindowState> m_WindowStates;
		std::vector<Id> m_WindowOrder;	// 背面（先頭）→前面（末尾）

		InputState m_Input;
		InputState m_PrevInput;

		Id m_FocusedWindow = 0;
		Id m_DraggedWindow = 0;
		DirectX::XMFLOAT2 m_DragOffset{};
		DirectX::XMFLOAT2 m_DragStartMousePos{};

		WindowFrame* m_CurrentWindow = nullptr;
		std::vector<std::unique_ptr<WindowFrame>> m_ActiveWindowFrame;	// このフレームでBeginされた分のみ生存

		CompositedFrame m_CompositedFrame;

		Id m_ResizeWindow = 0;
		DirectX::XMFLOAT2 m_ResizeStartMouse;
		DirectX::XMFLOAT2 m_ResizeStartSize;

		Id m_DraggedScrollbarWindow = 0;

		// ドッキングタブを押している途中の状態
		Id m_PressedDockTabWindow = 0;
		int m_PressedDockTabLeaf = -1;

		// タブを押した瞬間のマウス位置
		DirectX::XMFLOAT2 m_PressedDockTabMousePos{};

		// Leaf左上からマウス位置までの差
		// アンドック時にウィンドウがマウス位置へワープするのを防ぐ
		DirectX::XMFLOAT2 m_PressedDockTabOffset{};
	};
}
