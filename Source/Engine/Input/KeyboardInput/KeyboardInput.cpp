// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "KeyboardInput.h"


void Input::KeyboardInput::Update(bool _acceptInput)
{
	// 現在状態を前フレームへ以降
	m_Previous = m_Current;

	// 非アクティブ時はすべて離れている扱い
	if (!_acceptInput)
	{
		m_Current.fill(false);
		return;
	}

	for (uint32_t virtualKey = 0; virtualKey < static_cast<uint32_t>(KeyCount); ++virtualKey)
	{
		m_Current[virtualKey] = (GetAsyncKeyState(static_cast<uint32_t>(virtualKey)) & 0x8000) != 0;
	}
}

void Input::KeyboardInput::Reset()
{
	m_Current.fill(false);
	m_Previous.fill(false);
}

bool Input::KeyboardInput::IsDown(uint32_t _virtualKey) const
{
	if (!IsValidKey(_virtualKey))
	{
		return false;
	}

	return m_Current[_virtualKey];
}

bool Input::KeyboardInput::IsPressed(uint32_t _virtualKey) const
{
	if (!IsValidKey(_virtualKey))
	{
		return false;
	}
	
	return m_Current[_virtualKey] && !m_Previous[_virtualKey];
}

bool Input::KeyboardInput::IsReleased(uint32_t _virtualKey) const
{
	if (!IsValidKey(_virtualKey))
	{
		return false;
	}

	return !m_Current[_virtualKey] && m_Previous[_virtualKey];
}

bool Input::KeyboardInput::IsValidKey(uint32_t _virtualKey) const
{
	return _virtualKey < KeyCount;
}
