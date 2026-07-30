// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "MouseInput.h"

void Input::MouseInput::Update(HWND _hWnd, bool _acceptInput)
{
	m_PreviousButtons = m_CurrentButtons;

	if (!_acceptInput || _hWnd == nullptr)
	{
		m_CurrentButtons.fill(false);
		m_Delta = { 0,0 };
		return;
	}

	for (size_t i = 0; i < ButtonCount; ++i)
	{
		const auto button = static_cast<MouseButton>(i);
		const int virtualKey = ToVirtualKey(button);
		m_CurrentButtons[i] = (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
	}

	POINT screenPosition{};

	if (!GetCursorPos(&screenPosition))
	{
		m_Delta = { 0,0 };
		return;
	}

	if (m_IsRelativeMode)
	{
		const POINT centerClient = GetClientCenter(_hWnd);
		const POINT centerScreen = ClientToScreenPoint(_hWnd, centerClient);

		// 画面中央からどれだけ動いたか
		m_Delta.x = screenPosition.x - centerScreen.x;
		m_Delta.y = screenPosition.y - centerScreen.y;

		m_Position				= centerClient;
		m_PreviousPosition		= centerClient;
		m_HasPreviousPosition	= true;

		// 毎フレーム中央へ戻す
		SetCursorPos(centerScreen.x, centerScreen.y);
	}
	else
	{
		POINT clientPosition = screenPosition;
		ScreenToClient(_hWnd, &clientPosition);

		m_Position = clientPosition;

		if (!m_HasPreviousPosition)
		{
			m_PreviousPosition		= clientPosition;
			m_HasPreviousPosition	= true;
		}

		m_Delta.x = clientPosition.x - m_PreviousPosition.x;
		m_Delta.y = clientPosition.y - m_PreviousPosition.y;
		m_PreviousPosition = clientPosition;
	}

}

void Input::MouseInput::Reset()
{
	m_CurrentButtons.fill(false);
	m_PreviousButtons.fill(false);

	m_Position = { 0,0 };
	m_PreviousPosition = { 0,0 };
	m_Delta = { 0,0 };

	m_HasPreviousPosition = false;
}

bool Input::MouseInput::IsDown(MouseButton _button) const
{
	return m_CurrentButtons[ToIndex(_button)];
}

bool Input::MouseInput::IsPressed(MouseButton _button) const
{
	const size_t index = ToIndex(_button);

	return m_CurrentButtons[index] && !m_PreviousButtons[index];
}

bool Input::MouseInput::IsReleased(MouseButton _button) const
{
	const size_t index = ToIndex(_button);

	return !m_CurrentButtons[index] && m_PreviousButtons[index];
}

void Input::MouseInput::SetRelativeMode(HWND _hWnd, bool _enabled)
{
	if (_hWnd == nullptr || m_IsRelativeMode == _enabled) 
	{ return; }

	m_IsRelativeMode = _enabled;
	m_Delta = { 0,0 };

	if (_enabled)
	{
		SetCapture(_hWnd);

		// カーソルをクライアント領域に制限
		RECT clientRect{};
		GetClientRect(_hWnd, &clientRect);

		POINT topLeft
		{
			clientRect.left,
			clientRect.top
		};

		POINT bottomRight
		{
			clientRect.right,
			clientRect.bottom
		};

		ClientToScreen(_hWnd, &topLeft);
		ClientToScreen(_hWnd, &bottomRight);

		RECT screenRect
		{
			topLeft.x,
			topLeft.y,
			bottomRight.x,
			bottomRight.y
		};

		ClipCursor(&screenRect);

		// 中央へ移動
		const POINT centerClient = GetClientCenter(_hWnd);
		const POINT centerScreen = ClientToScreenPoint(_hWnd, centerClient);

		SetCursorPos(centerScreen.x, centerScreen.y);

		m_Position = centerClient;
		m_PreviousPosition = centerClient;
		m_HasPreviousPosition = true;

		if (!m_IsCursorHidden)
		{
			ShowCursor(FALSE);
			m_IsCursorHidden = true;
		}
	}
	else
	{
		if (GetCapture() == _hWnd)
		{ ReleaseCapture(); }

		ClipCursor(nullptr);

		if (m_IsCursorHidden)
		{
			ShowCursor(TRUE);
			m_IsCursorHidden = false;
		}
		m_HasPreviousPosition = false;
	}
}

int Input::MouseInput::ToVirtualKey(MouseButton _button)
{
	switch (_button)
	{
	case MouseButton::Left:
		return VK_LBUTTON;
	case MouseButton::Right:
		return VK_RBUTTON;
	case MouseButton::Middle:
		return VK_MBUTTON;
	case MouseButton::X1:
		return VK_XBUTTON1;
	case MouseButton::X2:
		return VK_XBUTTON2;
	default :
		return 0;
	}
}

std::size_t Input::MouseInput::ToIndex(MouseButton _button)
{
	return static_cast<std::size_t>(_button);
}

POINT Input::MouseInput::GetClientCenter(HWND _hWnd) const
{
	RECT clientRect{};
	GetClientRect(_hWnd, &clientRect);

	return { (clientRect.left + clientRect.right) / 2, (clientRect.top + clientRect.bottom) / 2 };
}

POINT Input::MouseInput::ClientToScreenPoint(HWND _hWnd, POINT _point) const
{
	ClientToScreen(_hWnd, &_point);
	return _point;
}
