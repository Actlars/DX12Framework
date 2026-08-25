// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "TextLayout.h"
#include <Engine/EditorUI/Text/Font/Font.h>

namespace
{
	// 1文字分の送り幅。アトラスに載せられなかった文字は幅0として無視する
	inline float GetAdvance(EditorUI::Font& _font, wchar_t _ch)
	{
		const EditorUI::Glyph* pGlyph = _font.GetGlyph(_ch);
		return (pGlyph != nullptr) ? pGlyph->Advance : 0.0f;
	}
}

// -------------------------------------------------------------------------------
// 1行分の表示幅を測る
// -------------------------------------------------------------------------------
float EditorUI::TextLayout::MeasureWidth(Font& _font, std::wstring_view _text)
{
	return MeasureWidthUpTo(_font, _text, _text.size());
}

// -------------------------------------------------------------------------------
// 先頭から_count文字分の表示幅を測る
// -------------------------------------------------------------------------------
float EditorUI::TextLayout::MeasureWidthUpTo(Font& _font, std::wstring_view _text, std::size_t _count)
{
	const std::size_t count = (std::min)(_count, _text.size());

	float width = 0.0f;
	for (std::size_t i = 0; i < count; ++i)
	{
		if (_text[i] == L'\n')
		{
			break;	// 改行より先は別の行なので、この行の幅には含めない
		}

		width += GetAdvance(_font, _text[i]);
	}

	return width;
}

// -------------------------------------------------------------------------------
// X座標から文字の区切り位置を求める
// -------------------------------------------------------------------------------
std::size_t EditorUI::TextLayout::FindCharacterIndexAtX(Font& _font, std::wstring_view _text, float _offsetX)
{
	if (_offsetX <= 0.0f)
	{
		return 0;
	}

	float width = 0.0f;
	for (std::size_t i = 0; i < _text.size(); ++i)
	{
		const float advance = GetAdvance(_font, _text[i]);

		// 文字の中心より左をクリックしたらその文字の手前、右なら後ろにキャレットを置く
		// こうすると、文字の左半分と右半分でキャレットの行き先が直感通りになる
		if (_offsetX < width + advance * 0.5f)
		{
			return i;
		}

		width += advance;
	}

	return _text.size();
}

// -------------------------------------------------------------------------------
// 文字列を行に分割する
// -------------------------------------------------------------------------------
EditorUI::TextMetrics EditorUI::TextLayout::Build(
	Font&					_font,
	std::wstring_view		_text,
	const TextOptions&		_options,
	float					_maxWidth,
	std::vector<TextLine>&	_outLines)
{
	_outLines.clear();

	TextMetrics metrics;

	const float lineHeight = _font.GetLineHeight() + _options.LineSpacing;

	// 折り返し幅の決定。明示指定を優先し、なければ与えられた領域幅を使う
	const float wrapWidth = (_options.WrapWidth > 0.0f) ? _options.WrapWidth : _maxWidth;

	// 折り返しも省略もしないなら幅の制限は不要なので、無限大として扱う
	const bool  limitWidth	= _options.Wrap || _options.Ellipsis;
	const float lineWidthLimit = (limitWidth && wrapWidth > 0.0f) ? wrapWidth : FLT_MAX;

	uint32_t cursor		= 0;
	float	 maxWidth	= 0.0f;

	while (cursor <= _text.size())
	{
		// 行数の上限に達したら、残りは省略されたものとして扱う
		if (_options.MaxLines != 0 && metrics.LineCount >= _options.MaxLines)
		{
			metrics.Truncated = true;

			// 最後の行の末尾を省略記号に差し替え、途切れていることを示す
			if (_options.Ellipsis && !_outLines.empty())
			{
				_outLines.back().Ellipsis = true;
				ApplyEllipsis(_font, _text, lineWidthLimit, _outLines.back());
			}
			break;
		}

		TextLine line;
		line.Begin = cursor;

		float lineWidth = 0.0f;
		const uint32_t next = _options.Wrap
			? FindLineBreak(_font, _text, cursor, lineWidthLimit, lineWidth)
			: FindLineBreak(_font, _text, cursor, FLT_MAX, lineWidth);

		// 行の終端は、改行文字を含まない位置にそろえる
		line.End	= next;
		line.Width	= lineWidth;

		while (line.End > line.Begin && _text[line.End - 1] == L'\n')
		{
			--line.End;
		}

		// 折り返さない設定でも、幅に収まらなければ省略記号で切り詰める
		if (!_options.Wrap && _options.Ellipsis && line.Width > lineWidthLimit)
		{
			metrics.Truncated	= true;
			line.Ellipsis		= true;
			ApplyEllipsis(_font, _text, lineWidthLimit, line);
		}

		_outLines.push_back(line);
		maxWidth = (std::max)(maxWidth, line.Width);
		++metrics.LineCount;

		if (next >= _text.size())
		{
			break;	// 末尾まで到達した
		}

		cursor = next;
	}

	metrics.Size =
	{
		maxWidth,
		static_cast<float>(metrics.LineCount) * lineHeight
	};

	return metrics;
}

// -------------------------------------------------------------------------------
// 行揃えのオフセットを求める
// -------------------------------------------------------------------------------
float EditorUI::TextLayout::ResolveAlignmentOffsetX(
	TextHorizontalAlignment _alignment,
	float					_availableWidth,
	float					_lineWidth)
{
	// はみ出している行を左へずらすと先頭が読めなくなるため、余白は負にしない
	const float remain = (std::max)(0.0f, _availableWidth - _lineWidth);

	switch (_alignment)
	{
	case TextHorizontalAlignment::Center:	return remain * 0.5f;
	case TextHorizontalAlignment::Right:	return remain;
	case TextHorizontalAlignment::Left:
	default:								return 0.0f;
	}
}

// -------------------------------------------------------------------------------
// 1行の終端位置を求める
// -------------------------------------------------------------------------------
uint32_t EditorUI::TextLayout::FindLineBreak(
	Font&				_font,
	std::wstring_view	_text,
	uint32_t			_begin,
	float				_maxWidth,
	float&				_outWidth)
{
	const uint32_t length = static_cast<uint32_t>(_text.size());

	float		width			= 0.0f;
	uint32_t	lastBreakPos	= 0;		// 直近の「ここで折り返してよい」位置
	float		lastBreakWidth	= 0.0f;

	for (uint32_t i = _begin; i < length; ++i)
	{
		const wchar_t ch = _text[i];

		// 明示的な改行はここで確定。改行文字自体を含めて次の行へ進める
		if (ch == L'\n')
		{
			_outWidth = width;
			return i + 1;
		}

		const float advance = GetAdvance(_font, ch);

		// 1文字目だけは、幅に収まらなくても必ず入れる
		// そうしないと1文字も進まないまま無限ループになる
		if (width + advance > _maxWidth && i > _begin)
		{
			// 折り返してよい位置を覚えていればそこまで戻る（単語の途中で切らない）
			if (lastBreakPos > _begin)
			{
				_outWidth = lastBreakWidth;

				// 行頭に残っても意味のない空白は読み飛ばす
				uint32_t next = lastBreakPos;
				while (next < length && IsSpace(_text[next]))
				{
					++next;
				}
				return next;
			}

			// 戻れる位置がない（1単語が長すぎる）ので、その場で強制的に切る
			_outWidth = width;
			return i;
		}

		width += advance;

		if (IsBreakableAfter(ch))
		{
			lastBreakPos	= i + 1;
			lastBreakWidth	= width;
		}
	}

	_outWidth = width;
	return length;
}

// -------------------------------------------------------------------------------
// 省略記号が収まるところまで行を切り詰める
// -------------------------------------------------------------------------------
void EditorUI::TextLayout::ApplyEllipsis(
	Font&				_font,
	std::wstring_view	_text,
	float				_maxWidth,
	TextLine&			_line)
{
	const float ellipsisWidth = MeasureWidth(_font, kEllipsis);

	// 省略記号すら入らない狭さなら、文字を全部落として省略記号だけを描く
	if (_maxWidth <= ellipsisWidth)
	{
		_line.End	= _line.Begin;
		_line.Width	= ellipsisWidth;
		return;
	}

	const float textLimit = _maxWidth - ellipsisWidth;

	float width = 0.0f;
	uint32_t end = _line.Begin;

	for (uint32_t i = _line.Begin; i < _line.End; ++i)
	{
		const float advance = GetAdvance(_font, _text[i]);
		if (width + advance > textLimit)
		{
			break;
		}

		width += advance;
		end = i + 1;
	}

	_line.End	= end;
	_line.Width	= width + ellipsisWidth;
}

// -------------------------------------------------------------------------------
// この文字の直後で折り返してよいか
// -------------------------------------------------------------------------------
bool EditorUI::TextLayout::IsBreakableAfter(wchar_t _ch)
{
	if (IsSpace(_ch))
	{
		return true;
	}

	// 単語の区切りとして扱う記号
	if (_ch == L'-' || _ch == L'/' || _ch == L',')
	{
		return true;
	}

	// -------------------------------------------------------------------------------
	// 日本語・中国語・韓国語は単語を空白で区切らないため、文字単位で折り返す
	// 主要な範囲だけを対象にしている
	//		U+3040-U+30FF : ひらがな・カタカナ
	//		U+3400-U+4DBF : CJK統合漢字拡張A
	//		U+4E00-U+9FFF : CJK統合漢字
	//		U+AC00-U+D7AF : ハングル音節
	//		U+FF00-U+FFEF : 全角英数・半角カナ
	// -------------------------------------------------------------------------------
	return (_ch >= 0x3040 && _ch <= 0x30FF) ||
		   (_ch >= 0x3400 && _ch <= 0x4DBF) ||
		   (_ch >= 0x4E00 && _ch <= 0x9FFF) ||
		   (_ch >= 0xAC00 && _ch <= 0xD7AF) ||
		   (_ch >= 0xFF00 && _ch <= 0xFFEF);
}

bool EditorUI::TextLayout::IsSpace(wchar_t _ch)
{
	return _ch == L' ' || _ch == L'\t';
}
