#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Types.h>
#include <Engine/EditorUI/Core/DrawList/DrawList.h>
#include <Engine/EditorUI/Core/Window/WindowFlags.h>
#include <Engine/EditorUI/Core/Window/WindowState.h>

namespace EditorUI
{
	// -------------------------------------------------------------------------------
	// WindowFrame struct
	// 
	// 概要 : 
	//	BeginWindow ～ EndWindowの間だけ生存する一時状態のWindow
	//	永続状態（WindowState）と、今フレームのレイアウト/DrawListを明確に分離する
	// -------------------------------------------------------------------------------
	struct WindowFrame
	{
		WindowState*		pState = nullptr;
		DrawList			Draw;
		WindowFlags			Flags = WindowFlags::None;

		// 今フレーム実際に描画する矩形情報。Dock時はDock Leaf、Floating時はState由来。
		Rect2D	WindowRect{};	// このフレームで実際に描画に使った矩形
		Rect2D  ContentRect{};
		float	ChromeHeight	= 0.0f;
		bool	IsDocked		= false;

		// Collapsed時はWindowChromeだけを描画し、Widget内容を処理しない
		bool	SkipContents	= false;

		// DockLeafの非ActiveTab。Window全体を描画しない
		bool	SkippedEntirely = false;

		// -------------------------------------------------------------------------------
		// Layout State
		// -------------------------------------------------------------------------------
		DirectX::XMFLOAT2	CursorPos{};
		DirectX::XMFLOAT2	ContentOrigin{};
		float				LineHeight		= 0.0f;
		float				ContentHeight	= 0.0f;		// このフレームで実際に使われたコンテンツの高さ
		float				ContentWidthUsed = 0.0f;	// このフレームで実際に使われたコンテンツの幅(自動サイズ用)
		float				IndentX			= 0.0f;		// ツリーの階層に応じた左インデント量

		// このウィンドウで使う余白
		// 既定はStyle::WindowPaddingだが、メニューバーのように
		// 高さが決まっている帯では小さくしたいので、フレーム単位で持つ
		DirectX::XMFLOAT2	Padding{ 0.0f, 0.0f };

		Rect2D				LastItemRect{};				// スクロール反映後のスクリーン座標
		float				LineY			= 0.0f;		// スクロール前の仮想座標
		bool				HasLastItem		= false;	// 同じウィンドウ内でウィジェットを1つ以上置いたか

		// 次に置くウィジェットの幅。0以下なら「コンテンツ領域いっぱい」を意味する
		// SetNextItemWidthで1回だけ指定でき、消費された時点で0に戻る
		float	NextItemWidth = 0.0f;
	};
}
