// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "KeyboardInput.h"

namespace
{
	// 1フレームに受け付ける入力文字数の上限
	// ウィンドウメッセージが滞留したときに、際限なく積み上がるのを防ぐ
	constexpr std::size_t kMaxInputCharactersPerFrame = 64;
}

void Input::KeyboardInput::Update(bool _acceptInput)
{
	// 現在状態を前フレームへ以降
	m_Previous = m_Current;

	// メッセージ経由で溜めた分を、このフレームの確定分として取り出す
	m_KeyDownEvents = m_PendingKeyDownEvents;
	m_PendingKeyDownEvents.fill(false);

	m_InputCharacters = std::move(m_PendingInputCharacters);
	m_PendingInputCharacters.clear();

	// 非アクティブ時はすべて離れている扱い
	if (!_acceptInput)
	{
		m_Current.fill(false);
		m_KeyDownEvents.fill(false);
		m_InputCharacters.clear();
		return;
	}

	for (uint32_t virtualKey = 0; virtualKey < static_cast<uint32_t>(KeyCount); ++virtualKey)
	{
		m_Current[virtualKey] = (GetAsyncKeyState(static_cast<int>(virtualKey)) & 0x8000) != 0;
	}
}

void Input::KeyboardInput::Reset()
{
	m_Current.fill(false);
	m_Previous.fill(false);

	m_PendingKeyDownEvents.fill(false);
	m_KeyDownEvents.fill(false);

	m_PendingInputCharacters.clear();
	m_InputCharacters.clear();
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

void Input::KeyboardInput::AddKeyDownEvent(uint32_t _virtualKey)
{
	if (!IsValidKey(_virtualKey))
	{
		return;
	}

	m_PendingKeyDownEvents[_virtualKey] = true;
}

void Input::KeyboardInput::AddInputCharacter(wchar_t _character)
{
	if (m_PendingInputCharacters.size() >= kMaxInputCharactersPerFrame)
	{
		return;
	}

	m_PendingInputCharacters.push_back(_character);
}

bool Input::KeyboardInput::HasKeyDownEvent(uint32_t _virtualKey) const
{
	if (!IsValidKey(_virtualKey))
	{
		return false;
	}

	return m_KeyDownEvents[_virtualKey];
}

bool Input::KeyboardInput::IsValidKey(uint32_t _virtualKey) const
{
	return _virtualKey < KeyCount;
}
