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
	//
	//	色はすべて MakeColor(R, G, B, A) で書く
	//	16進で直接書くと、頂点フォーマット(R8G8B8A8)とリトルエンディアンの都合で
	//	赤と青が入れ替わるため、必ずこのヘルパー経由で指定する
	// -------------------------------------------------------------------------------
	struct Style
	{
		// -------------------------------------------------------------------------------
		// ウィンドウ本体・タイトルバー・枠線などの基本配色
		// UE5のエディタに近い、彩度を落とした暗いグレーを基調にする
		// -------------------------------------------------------------------------------
		Color32 ColorWindowBg			= MakeColor( 38,  38,  40);	// ウィンドウ本体
		Color32 ColorPanelBg			= MakeColor( 30,  30,  32);	// パネルの一段暗い背景(リスト等)
		Color32 ColorTitleBarBg			= MakeColor( 52,  52,  55);	// ウィンドウタイトル
		Color32 ColorTitleBarBgFocused	= MakeColor( 62,  84, 122);	// 選択ウィンドウタイトル
		Color32 ColorBorder				= MakeColor( 22,  22,  24);	// 枠線
		Color32 ColorBorderLight		= MakeColor( 68,  68,  72);	// 区切り線など、控えめな枠線

		// -------------------------------------------------------------------------------
		// テキスト
		//
		// 純白(255,255,255)は暗い背景の上で滲んで見えるうえ、長時間の作業でまぶしい
		// 主役のテキストはわずかに輝度を落とした、青みのあるオフホワイトにしている
		// -------------------------------------------------------------------------------
		Color32 ColorText				= MakeColor(208, 211, 216);	// 通常テキスト(まぶしくないオフホワイト)
		Color32 ColorTextBright			= MakeColor(232, 234, 238);	// 見出しなど、特に目立たせたいテキスト
		Color32 ColorTextMuted			= MakeColor(146, 150, 158);	// 補足説明など、主役ではないテキスト
		Color32 ColorTextDisabled		= MakeColor(104, 108, 114);	// ReadOnly時のテキスト

		// ボタン等のインタラクティブ要素用
		Color32 ColorButton				= MakeColor( 66,  66,  70);	// ボタン
		Color32 ColorButtonHovered		= MakeColor( 84,  84,  90);	// マウスが乗っているか
		Color32 ColorButtonActive		= MakeColor(100, 100, 108);	// マウスが押されているか

		// 値編集ウィジェット(InputText / Drag系)の入力欄
		Color32 ColorFrameBg			= MakeColor( 27,  27,  29);	// 入力欄の背景
		Color32 ColorFrameBgHovered		= MakeColor( 40,  40,  44);	// マウスが乗っているとき
		Color32 ColorFrameBgActive		= MakeColor( 50,  50,  55);	// 編集中・ドラッグ中
		Color32 ColorFrameBgReadOnly	= MakeColor( 34,  34,  36);	// ReadOnly時
		Color32 ColorTextCursor			= MakeColor(232, 234, 238);	// テキストキャレット
		Color32 ColorTextSelectionBg	= MakeColor( 58,  110, 165);// テキスト選択範囲

		// -------------------------------------------------------------------------------
		// 一覧表示(ヒエラルキー / コンテンツブラウザ / メニュー)用
		// 「マウスが乗っている」と「選択されている」を明確に区別できる強さにする
		// -------------------------------------------------------------------------------
		Color32 ColorHeader				= MakeColor( 52,  56,  64);	// 折り畳み見出しの背景
		Color32 ColorItemHovered		= MakeColor( 60,  64,  72);	// 行のホバー
		Color32 ColorItemSelected		= MakeColor( 46,  86, 134);	// 行の選択
		Color32 ColorItemSelectedInactive = MakeColor( 58,  62,  70);	// 選択中だがフォーカスが別にある

		// XYZWの各成分を見分けるためのアクセント色(XMFLOAT2/3/4のProperty用)
		// UE5と同じく X=赤 / Y=緑 / Z=青 / W=灰 に対応させる
		Color32 ColorAxisX				= MakeColor(198,  76,  76);
		Color32 ColorAxisY				= MakeColor( 92, 168,  92);
		Color32 ColorAxisZ				= MakeColor( 76, 116, 200);
		Color32 ColorAxisW				= MakeColor(140, 140, 148);

		// スクロール
		float ScrollbarWidth				= 8.0f;
		Color32 ColorScrollbarBg			= MakeColor( 30,  30,  32);
		Color32 ColorScrollbarThumb			= MakeColor( 84,  84,  90);
		Color32 ColorScrollbarThumbHovered	= MakeColor(110, 110, 118);

		// -------------------------------------------------------------------------------
		// ドッキング
		// -------------------------------------------------------------------------------
		Color32 ColorTabBg				= MakeColor( 44,  44,  47);	// 非アクティブなタブ
		Color32 ColorTabHovered			= MakeColor( 72,  72,  78);
		Color32 ColorTabActive			= MakeColor( 62,  84, 122);	// アクティブなタブ
		Color32 ColorTabBarBg			= MakeColor( 30,  30,  32);	// タブが並ぶ帯の背景
		Color32 ColorDockPreview		= MakeColor( 74, 132, 200, 110);	// ドロップ先の半透明プレビュー
		Color32 ColorDockPreviewBorder	= MakeColor(120, 176, 240, 220);	// プレビューの枠線

		// レイアウトにかかわる寸法値。
		float TitleBarHeight	= 24.0f;
		float WindowPadding		= 8.0f;		// ウィンドウ枠とコンテンツの余白
		float ItemSpacing		= 4.0f;		// ウィジェット間の縦方向の間隔
		float ItemInnerSpacing	= 4.0f;		// 1つのウィジェットを構成する部品どうしの間隔
		float BorderThickness	= 1.0f;
		float ResizeGripSize	= 12.0f;

		float SplitterHitThickness	= 6.0f;	// 当たり判定の許容幅
		float SplitterThickness		= 2.0f;	// 見た目の線の太さ

		// 値編集ウィジェット用の寸法値
		float FramePaddingX		= 6.0f;		// 入力欄の枠とテキストの余白(横)
		float FramePaddingY		= 3.0f;		// 入力欄の枠とテキストの余白(縦)
		float TextCursorWidth	= 1.0f;		// キャレットの太さ

		// 一覧・ツリー用の寸法値
		float TreeIndentWidth	= 14.0f;	// 階層1段あたりのインデント量
		float TreeArrowSize		= 9.0f;		// 開閉を示す三角形の一辺
		float RowPaddingY		= 2.0f;		// 1行の上下余白

		// ドッキングのタブ
		float TabMinWidth		= 80.0f;
		float TabMaxWidth		= 180.0f;
		float TabPaddingX		= 10.0f;
		float TabCloseButtonSize = 12.0f;

		// メニュー / ポップアップ
		float MenuItemHeight	= 22.0f;
		float MenuPaddingX		= 10.0f;
		float MenuMinWidth		= 160.0f;

		// クリックとドラッグを見分ける移動量のしきい値
		// これ未満の移動で離した場合はクリック（＝キーボード入力へ切り替え）とみなす
		float DragActivationThreshold = 3.0f;

		// ウィンドウを画面の外へ運んだとき、外周ドッキングに切り替わるまでの猶予(ピクセル)
		// 0にすると画面際で誤爆するため、少しだけ外に出てから反応させる
		float ScreenEdgeDockMargin = 8.0f;
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
