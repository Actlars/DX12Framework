#pragma once

namespace Input
{
	// -------------------------------------------------------------------------------
	// KeyboardInput class
	//
	// 概要 :
	//	キーボードの状態を2通りの方法で保持するクラス
	//
	//	ポーリング(Update)
	//		毎フレームすべてのキーの押下状態を取り込む
	//		「押しているか」を知りたい移動操作などに向く
	//
	//	メッセージ(ProcessWindowMessage経由のAddXxx)
	//		Win32から届いたWM_KEYDOWN / WM_CHARを取りこぼさずに溜める
	//		オートリピートやIMEを通した確定文字は、ポーリングでは再現できないため
	//		テキスト入力はこちらを使う
	// -------------------------------------------------------------------------------
	class KeyboardInput
	{
	public:

		static constexpr std::size_t KeyCount = 256;

		void Update(bool _acceptInput);
		void Reset();

		bool IsDown		(uint32_t _virtualKey)	const;	// 押し中
		bool IsPressed	(uint32_t _virtualKey)	const;	// 押した瞬間
		bool IsReleased	(uint32_t _virtualKey)	const;	// 離した瞬間

		// -------------------------------------------------------------------------------
		// @brief	WM_KEYDOWNで届いた押下を記録する
		//
		//	キーを押しっぱなしにするとOSが繰り返し送ってくるため、
		//	IsPressedでは取れない「連続入力」をここで拾える
		//
		// @param[in]	_virtualKey		仮想キーコード
		// -------------------------------------------------------------------------------
		void AddKeyDownEvent(uint32_t _virtualKey);

		// -------------------------------------------------------------------------------
		// @brief	WM_CHARで届いた確定文字を記録する
		//
		// @param[in]	_character	入力された文字
		// -------------------------------------------------------------------------------
		void AddInputCharacter(wchar_t _character);

		// このフレームに押下通知が届いたか（オートリピートを含む）
		bool HasKeyDownEvent(uint32_t _virtualKey) const;

		// このフレームに入力された確定文字
		const std::wstring& GetInputCharacters() const { return m_InputCharacters; }

	private:

		// キーの最大数より入力値が大きければfalseを返す（存在しないキー）
		bool IsValidKey(uint32_t _virtualKey) const;

	private:

		std::array<bool, KeyCount> m_Current	{};
		std::array<bool, KeyCount> m_Previous	{};

		// -------------------------------------------------------------------------------
		// メッセージ経由で溜めた分
		//
		// 溜める側(WndProc)と読む側(Update後のゲームループ)でタイミングが異なるため、
		// Pendingへ溜めてからUpdateで確定分へ移す
		// これにより「1フレームの入力」の範囲が明確になり、取りこぼしも二重取得も起きない
		// -------------------------------------------------------------------------------
		std::array<bool, KeyCount>	m_PendingKeyDownEvents	{};
		std::array<bool, KeyCount>	m_KeyDownEvents			{};

		std::wstring m_PendingInputCharacters;
		std::wstring m_InputCharacters;
	};
}
