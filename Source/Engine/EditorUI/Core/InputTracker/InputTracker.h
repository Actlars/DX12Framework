#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Input.h>

namespace EditorUI
{
	// -------------------------------------------------------------------------------
	// InputTracker class
	// 
	// 概要 : 
	//	現在/前フレームのInputStateを所有し、Clicked/Released/Deltaなどの
	//	フレーム差分を一か所で導出する
	// -------------------------------------------------------------------------------
	class InputTracker
	{
	public:

		// -------------------------------------------------------------------------------
		// @brief	フレーム開始の初期化処理
		// 
		// @param[in]	_input	入力情報
		// -------------------------------------------------------------------------------
		void NewFrame(const InputState& _input);

		const InputState& Current()		const;
		const InputState& Previous()	const;

		const DirectX::XMFLOAT2&	GetMousePos()	const;
		DirectX::XMFLOAT2			GetMouseDelta() const;
		float						GetMouseWheel() const;

		bool IsMouseDown(MouseButton _button)		const;
		bool IsMouseClicked(MouseButton _button)	const;
		bool IsMouseReleased(MouseButton _button)	const;

		// -------------------------------------------------------------------------------
		// @brief	直前のクリックから短時間・近距離で再度クリックされたか
		//
		//	コンテンツブラウザで「ダブルクリックして開く」を実現するために使う
		// -------------------------------------------------------------------------------
		bool IsMouseDoubleClicked(MouseButton _button) const;

		// 経過時間(秒)と、起動からの累積時間(秒)
		float  GetDeltaTime()	const;
		double GetTime()		const;

		bool IsKeyDown(Key _key)	const;
		bool IsKeyPressed(Key _key) const;

		const std::wstring& GetInputCharacters() const;

	private:

		InputState m_Current{};
		InputState m_Previous{};

		// 起動からの累積時間。ダブルクリック判定の基準になる
		double m_Time = 0.0;

		// ボタンごとの、前回クリックした時刻と位置
		double				m_LastClickTime[kMouseButtonCount]{};
		DirectX::XMFLOAT2	m_LastClickPos[kMouseButtonCount]{};
		bool				m_DoubleClicked[kMouseButtonCount]{};

	};
}

