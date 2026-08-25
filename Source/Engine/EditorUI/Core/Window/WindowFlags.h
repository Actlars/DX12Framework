#pragma once

namespace EditorUI
{
	// -------------------------------------------------------------------------------
	// WindowFlags : enum
	// 
	// 概要 : 
	//	ウィンドウの見た目・ふるまいを切り替えるためのビットフラグ
	//	１つのウィンドウが「タイトルバーなし」かつ「リサイズ不可」のように
	//	複数の特性を同時にもてるようにするため、boolを何個も引数に並べる代わりに
	//	１つの整数の各ビットに意味を持たせている
	// -------------------------------------------------------------------------------
	enum class WindowFlags : uint32_t
	{
		None		= 0,
		NoTitleBar	= 1u << 0,
		NoResize	= 1u << 1,
		NoMove		= 1u << 2,
		NoScrollbar = 1u << 3,

		// ポップアップ(コンテキストメニュー等)であることを示す
		// すべてのウィンドウより手前に描かれ、ドッキングの対象にもならない
		Popup		= 1u << 4,

		// タイトルバーをドラッグしてもドッキングさせない
		// ゲームビューのように、常に決まった場所へ置きたいウィンドウで使う
		NoDock		= 1u << 5,

		// 背景と枠線を描かない。既に背景を持つ領域へ重ねて使う
		NoBackground = 1u << 6,

		// よく使う組み合わせに名前を付けておく
		Menu = NoTitleBar | NoResize | NoMove | NoScrollbar | Popup | NoDock,
	};

	// enum class は意図的に算術演算子が使えない設計になっている
	// ここで演算子を自分で定義することで、ビットフラグとして安全に合成できるようにする
	inline constexpr WindowFlags operator|(WindowFlags _a, WindowFlags _b)
	{
		return static_cast<WindowFlags>(static_cast<uint32_t>(_a) | static_cast<uint32_t>(_b));
	}
	inline constexpr WindowFlags operator&(WindowFlags _a, WindowFlags _b)
	{
		return static_cast<WindowFlags>(static_cast<uint32_t>(_a) & static_cast<uint32_t>(_b));
	}
	inline constexpr WindowFlags operator~(WindowFlags _flags)
	{
		return static_cast<WindowFlags>(~static_cast<uint32_t>(_flags));
	}
	inline constexpr WindowFlags& operator|=(WindowFlags& _a, WindowFlags _b)
	{
		_a = _a | _b;
		return _a;
	}
	inline constexpr WindowFlags& operator&=(WindowFlags& _a, WindowFlags _b)
	{
		_a = _a & _b;
		return _a;
	}
	inline constexpr bool HasFlag(WindowFlags _flags, WindowFlags _bit)
	{
		return (static_cast<uint32_t>(_flags) & static_cast<uint32_t>(_bit)) != 0;
	}
	inline constexpr bool HasAllFlags(WindowFlags _flags, WindowFlags _bit)
	{
		const auto flags	= static_cast<uint32_t>(_flags);
		const auto bits		= static_cast<uint32_t>(_bit);
		return (flags & bits) == bits;
	}
}