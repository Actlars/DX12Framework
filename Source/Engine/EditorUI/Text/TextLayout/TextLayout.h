#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Types.h>

namespace EditorUI
{
	class Font;

	// -------------------------------------------------------------------------------
	// TextHorizontalAlignment : enum
	//
	// 概要 :
	//	1行のテキストを、与えられた横幅のどこへ寄せるか
	// -------------------------------------------------------------------------------
	enum class TextHorizontalAlignment : uint8_t
	{
		Left,
		Center,
		Right,
	};

	// -------------------------------------------------------------------------------
	// TextOptions struct
	//
	// 概要 :
	//	テキストの計測と描画に共通する指定をまとめたもの
	//	MeasureTextとTextが必ず同じ構造体を受け取ることで、
	//	「計測した結果と実際の描画結果がずれる」ことがないようにしている
	// -------------------------------------------------------------------------------
	struct TextOptions
	{
		bool	Wrap		= false;	// 折り返しを行うか
		float	WrapWidth	= 0.0f;		// 折り返し幅。0以下ならコンテンツ領域の幅を使う
		float	LineSpacing = 0.0f;		// 行の高さに加算する余白

		TextHorizontalAlignment HorizontalAlignment = TextHorizontalAlignment::Left;

		uint32_t	MaxLines		= 0;	// 最大行数。0なら無制限
		bool		Ellipsis		= true;	// 入りきらない場合に末尾を "..." で置き換えるか
		bool		ClipToBounds	= true;	// 描画をレイアウト矩形でクリップするか

		// 個別に色を指定したい場合に使う
		// Color32単体では「指定なし」を表現できないため、有効フラグと対にしている
		bool		UseCustomColor	= false;
		Color32		Color			= 0xFFFFFFFFu;
	};

	// -------------------------------------------------------------------------------
	// TextMetrics struct
	//
	// 概要 :
	//	テキストを組んだ結果のサイズ情報
	// -------------------------------------------------------------------------------
	struct TextMetrics
	{
		DirectX::XMFLOAT2 Size{ 0.0f,0.0f };	// 組み上がった矩形のサイズ
		uint32_t	LineCount = 0;				// 実際に組まれた行数
		bool		Truncated = false;			// 行数制限などで一部が省略されたか
	};

	// -------------------------------------------------------------------------------
	// TextLine struct
	//
	// 概要 :
	//	組み上がった1行分の情報
	//	元の文字列への [Begin, End) の範囲で表すことで、行ごとに部分文字列を
	//	コピーせずに済ませている
	// -------------------------------------------------------------------------------
	struct TextLine
	{
		uint32_t	Begin	= 0;		// 元文字列における開始位置
		uint32_t	End		= 0;		// 元文字列における終端位置(この位置は含まない)
		float		Width	= 0.0f;		// 省略記号を含めた表示幅
		bool		Ellipsis = false;	// 末尾に省略記号を付けて描くか
	};

	// -------------------------------------------------------------------------------
	// TextLayout class
	//
	// 概要 :
	//	文字列を「どこで折り返し、どこを省略し、どの位置に描くか」まで解決するクラス
	//
	//	Fontがグリフ1文字分の情報を提供するのに対し、こちらは文字列全体の
	//	組版を担当する。描画そのものは行わないため、計測だけを目的とした
	//	呼び出し(MeasureText)と、描画のための呼び出し(Text)で同じ結果を共有できる
	// -------------------------------------------------------------------------------
	class TextLayout
	{
	public:

		// 省略時に末尾へ付ける文字列
		// U+2026(…)ではなくピリオド3つを使うのは、どのフォントでも確実に描けるため
		static constexpr std::wstring_view kEllipsis = L"...";

		// -------------------------------------------------------------------------------
		// @brief	改行を含まない1行分の表示幅を測る
		//
		// @param[in]	_font	計測に使うフォント
		// @param[in]	_text	計測する文字列
		// @return	表示幅(ピクセル)
		// -------------------------------------------------------------------------------
		static float MeasureWidth(Font& _font, std::wstring_view _text);

		// -------------------------------------------------------------------------------
		// @brief	指定した文字数までの表示幅を測る
		//
		//	テキスト入力欄が、キャレットを何ピクセル目に描くかを求めるために使う
		//
		// @param[in]	_font	計測に使うフォント
		// @param[in]	_text	計測する文字列
		// @param[in]	_count	先頭から数えた文字数
		// @return	表示幅(ピクセル)
		// -------------------------------------------------------------------------------
		static float MeasureWidthUpTo(Font& _font, std::wstring_view _text, std::size_t _count);

		// -------------------------------------------------------------------------------
		// @brief	表示幅上の位置から、最も近い文字の区切り位置を求める
		//
		//	入力欄をクリックしたときに、キャレットを置く位置を決めるために使う
		//
		// @param[in]	_font		計測に使うフォント
		// @param[in]	_text		対象の文字列
		// @param[in]	_offsetX	文字列先頭からの相対X座標
		// @return	文字単位のインデックス(0 ～ _text.size())
		// -------------------------------------------------------------------------------
		static std::size_t FindCharacterIndexAtX(Font& _font, std::wstring_view _text, float _offsetX);

		// -------------------------------------------------------------------------------
		// @brief	文字列を行に分割する
		//
		// @param[in]		_font		計測に使うフォント
		// @param[in]		_text		対象の文字列
		// @param[in]		_options	折り返しや行数制限の指定
		// @param[in]		_maxWidth	利用できる横幅。Wrapが無効でも省略判定に使う
		// @param[out]		_outLines	組み上がった行。呼び出し前の内容は破棄される
		// @return	組み上がった結果のサイズ情報
		// -------------------------------------------------------------------------------
		static TextMetrics Build(
			Font&					_font,
			std::wstring_view		_text,
			const TextOptions&		_options,
			float					_maxWidth,
			std::vector<TextLine>&	_outLines);

		// -------------------------------------------------------------------------------
		// @brief	行揃えの指定から、行の描画開始X座標のオフセットを求める
		//
		// @param[in]	_alignment		行揃え
		// @param[in]	_availableWidth	1行に使える横幅
		// @param[in]	_lineWidth		その行の実際の幅
		// @return	行の先頭に加算するX方向のオフセット
		// -------------------------------------------------------------------------------
		static float ResolveAlignmentOffsetX(
			TextHorizontalAlignment _alignment,
			float					_availableWidth,
			float					_lineWidth);

	private:

		// 1行に収まる文字数を求める。折り返し位置の探索もここで行う
		// @return 次の行の開始位置
		static uint32_t FindLineBreak(
			Font&				_font,
			std::wstring_view	_text,
			uint32_t			_begin,
			float				_maxWidth,
			float&				_outWidth);

		// 省略記号を含めて_maxWidthに収まるところまで行を切り詰める
		static void ApplyEllipsis(
			Font&				_font,
			std::wstring_view	_text,
			float				_maxWidth,
			TextLine&			_line);

		// 日本語のように単語区切りを持たない文字は、どこでも折り返してよい
		static bool IsBreakableAfter(wchar_t _ch);

		// 半角スペースのように、行頭に残さず捨ててよい文字
		static bool IsSpace(wchar_t _ch);
	};
}
