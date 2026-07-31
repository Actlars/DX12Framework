#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Types.h"

namespace EditorUI
{
	// -------------------------------------------------------------------------------
	// Style struct
	// 
	// 概要 : 
	//	色・寸法を一か所に集約する
	//	テーマ切り替えや配色調整をこの構造体だけの変更で完結させる
	// -------------------------------------------------------------------------------
	struct Style
	{
		// ウィンドウ本体・タイトルバー・枠線などの基本配色
		Color32 ColorWindowBg			= 0xFF2B2B2Bu;	// ウィンドウ本体
		Color32 ColorTitleBarBg			= 0xFF3C3C3Cu;	// ウィンドウタイトル
		Color32 ColorTitleBarBgFocused	= 0xFF4A6A9cu;	// 選択ウィンドウタイトル
		Color32 ColorBorder				= 0xFF1A1A1Au;	// 枠線
		Color32 ColorText				= 0xFFFFFFFFu;	// テキスト

		// ボタン等のインタラクティブ要素用
		Color32 ColorButton				= 0xFF4A4A4Au;	// ボタン
		Color32 ColorButtonHovered		= 0xFF5A5A5Au;	// マウスが乗っているか
		Color32 ColorButtonActive		= 0xFF6A6A6Au;	// マウスが押されているか

		// レイアウトにかかわる寸法値。
		float TitleBarHeight	= 24.0f;
		float WindowPadding		= 8.0f;		// ウィンドウ枠とコンテンツの余白
		float ItemSpacing		= 4.0f;		// ウィジェット間の縦方向の間隔
		float BorderThickness	= 1.0f;
		float ResizeGripSize	= 12.0f;
	};

	// どこからでも参照できるデフォルトスタイルを１つだけ用意する
	// Context::Context()がm_Style = GetDefaultStyle() で初期値をコピーする際に使う
	// Context自身がその後、SetStyle()で丸ごと差し替えられるので、この関数が返すのは
	// あくまで最初の一回だけ使われる初期値のひな型であり、実行中の状態ではない。
	inline const Style& GetDefaultStyle()
	{
		static Style s_Default{};
		return s_Default;
	}
}
