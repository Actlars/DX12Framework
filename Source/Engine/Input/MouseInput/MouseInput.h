#pragma once

namespace Input
{
	enum class MouseButton : std::size_t
	{
		Left = 0,
		Right,
		Middle,
		X1,
		X2,
		Max
	};

	class MouseInput
	{

	public:

		void Update(HWND _hWnd, bool _acceptInput);
		void Reset();

		bool IsDown		(MouseButton _button) const;
		bool IsPressed	(MouseButton _button) const;
		bool IsReleased	(MouseButton _button) const;

		POINT GetPosition() const 
		{ return m_Position; }

		POINT GetDelta() const 
		{ return m_Delta; }

		bool IsRelativeMode() const 
		{ return m_IsRelativeMode; }

		void SetRelativeMode(HWND _hWnd, bool _enabled);

	private:

		static int ToVirtualKey(MouseButton _button);
		static std::size_t ToIndex(MouseButton _button);

		POINT GetClientCenter(HWND _hWnd) const;
		POINT ClientToScreenPoint(HWND _hWnd, POINT _point) const;

	private:

		static constexpr std::size_t ButtonCount = static_cast<std::size_t>(MouseButton::Max);

		std::array<bool, ButtonCount> m_CurrentButtons	{};
		std::array<bool, ButtonCount> m_PreviousButtons	{};

		POINT m_Position{};
		POINT m_PreviousPosition{};
		POINT m_Delta{};

		bool m_HasPreviousPosition	= false;
		bool m_IsRelativeMode		= false;
		bool m_IsCursorHidden		= false;

	};
}