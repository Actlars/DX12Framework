#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Types.h"
#include "DrawList.h"
#include "Id.h"

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
	};

	// enum class は意図的に算術演算子が使えない設計になっている
	// ここで演算子を自分で定義することで、ビットフラグとして安全に合成できるようにする
	inline WindowFlags operator|(WindowFlags _a, WindowFlags _b)
	{
		return static_cast<WindowFlags>(static_cast<uint32_t>(_a) | static_cast<uint32_t>(_b));
	}
	inline bool HasFlag(WindowFlags _flags, WindowFlags _bit)
	{
		return (static_cast<uint32_t>(_flags) & static_cast<uint32_t>(_bit)) != 0;
	}

	// フレームをまたいで状態を保持するウィンドウ
	struct WindowState
	{
		Id WindowId = 0;
		DirectX::XMFLOAT2 Position	{ 60.0f,60.0f };
		DirectX::XMFLOAT2 Size		{ 320.0f,240.0f };
		DirectX::XMFLOAT2 Scroll	{ 0.0f,0.0f };
		bool Collapsed	= false;
		bool Active		= false;	// 今フレームでBeginWindowされたか
		int DockNodeId	= -1;		// 予約 : Docking実装時にドック先ノードIDを入れる
	};

	// Begin～Endの間だけ生存するウィンドウ
	struct WindowFrame
	{
		WindowState*		pState			= nullptr;
		DrawList			Draw;
		DirectX::XMFLOAT2	CursorPos{};
		DirectX::XMFLOAT2	ContentOrigin{};
		float				LineHeight		= 0.0f;
		WindowFlags			Flags			= WindowFlags::None;
		bool				SkipContents	= false;	// Collapsed時、中身のウェジット呼び出しをスキップするためのフラグ
	
		// レイアウト用
		Rect2D	LastItemBound{};	// 直前に置いたウィジェットの矩形(SameLineで使う)
		float	LineY = 0.0f;		// 現在の行のY座標(SameLineで同じ行に戻るために使う)
	};
}
