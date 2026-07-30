#pragma once

namespace Input
{
	class KeyboardInput
	{
	public:

		static constexpr std::size_t KeyCount = 256;

		void Update(bool _acceptInput);
		void Reset();

		bool IsDown		(uint32_t _virtualKey)	const;	// 押し中
		bool IsPressed	(uint32_t _virtualKey)	const;	// 押した瞬間
		bool IsReleased	(uint32_t _virtualKey)	const;	// 離した瞬間

	private:

		// キーの最大数より入力値が大きければfalseを返す（存在しないキー）
		bool IsValidKey(uint32_t _virtualKey) const;

	private:

		std::array<bool, KeyCount> m_Current	{};
		std::array<bool, KeyCount> m_Previous	{};

	};
}