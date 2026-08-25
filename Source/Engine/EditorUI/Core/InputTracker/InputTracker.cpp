#include "InputTracker.h"

namespace
{
	// ダブルクリックとみなす時間差(秒)と、許容するマウスのぶれ(ピクセル)
	constexpr double kDoubleClickTime		= 0.30;
	constexpr float  kDoubleClickMaxDistance = 6.0f;
}

// -------------------------------------------------------------------------------
//	フレーム開始時の初期化処理
// -------------------------------------------------------------------------------
void EditorUI::InputTracker::NewFrame(const EditorUI::InputState& _input)
{
	m_Previous	= m_Current;
	m_Current	= _input;

	// 経過時間を積み上げ、この時刻を基準にダブルクリックを判定する
	m_Time += static_cast<double>(_input.DeltaTime);

	for (std::size_t i = 0; i < kMouseButtonCount; ++i)
	{
		m_DoubleClicked[i] = false;

		const bool clicked = m_Current.MouseDown[i] && !m_Previous.MouseDown[i];
		if (!clicked)
		{ continue; }

		const float dx = m_Current.MousePos.x - m_LastClickPos[i].x;
		const float dy = m_Current.MousePos.y - m_LastClickPos[i].y;

		const bool inTime	= (m_Time - m_LastClickTime[i]) <= kDoubleClickTime;
		const bool inRange	= (dx * dx + dy * dy) <= (kDoubleClickMaxDistance * kDoubleClickMaxDistance);

		m_DoubleClicked[i] = inTime && inRange;

		// 3回目を続けてダブルクリック扱いにしないよう、成立したら基準時刻をリセットする
		m_LastClickTime[i]	= m_DoubleClicked[i] ? 0.0 : m_Time;
		m_LastClickPos[i]	= m_Current.MousePos;
	}
}

const EditorUI::InputState& EditorUI::InputTracker::Current() const
{
	return m_Current;
}

const EditorUI::InputState& EditorUI::InputTracker::Previous() const
{
	return m_Previous;
}

const DirectX::XMFLOAT2& EditorUI::InputTracker::GetMousePos() const
{
	return m_Current.MousePos;
}

// -------------------------------------------------------------------------------
// マウスの移動量のフレーム差分を返す
// -------------------------------------------------------------------------------
DirectX::XMFLOAT2 EditorUI::InputTracker::GetMouseDelta() const
{
	return
	{
		m_Current.MousePos.x - m_Previous.MousePos.x,
		m_Current.MousePos.y - m_Previous.MousePos.y
	};
}

float EditorUI::InputTracker::GetMouseWheel() const
{
	return m_Current.MouseWheel;
}

bool EditorUI::InputTracker::IsMouseDown(EditorUI::MouseButton _button) const
{
	return m_Current.MouseDown[ToIndex(_button)];
}

bool EditorUI::InputTracker::IsMouseClicked(EditorUI::MouseButton _button) const
{
	const std::size_t index = ToIndex(_button);
	return m_Current.MouseDown[index] && !m_Previous.MouseDown[index];
}

bool EditorUI::InputTracker::IsMouseReleased(EditorUI::MouseButton _button) const
{
	const std::size_t index = ToIndex(_button);
	return !m_Current.MouseDown[index] && m_Previous.MouseDown[index];
}

bool EditorUI::InputTracker::IsMouseDoubleClicked(EditorUI::MouseButton _button) const
{
	return m_DoubleClicked[ToIndex(_button)];
}

float EditorUI::InputTracker::GetDeltaTime() const
{
	return m_Current.DeltaTime;
}

double EditorUI::InputTracker::GetTime() const
{
	return m_Time;
}

bool EditorUI::InputTracker::IsKeyDown(EditorUI::Key _key) const
{
	return m_Current.IsKeyDown(_key);
}

bool EditorUI::InputTracker::IsKeyPressed(EditorUI::Key _key) const
{
	return m_Current.IsKeyPressed(_key);
}

const std::wstring& EditorUI::InputTracker::GetInputCharacters() const
{
	return m_Current.InputCharacters;
}
