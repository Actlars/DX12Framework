// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Widgets.h"
#include <Engine/EditorUI/Widgets/Layout/Layout.h>
#include <Engine/EditorUI/Widgets/WidgetInteraction.h>
#include <Engine/EditorUI/Widgets/WidgetPrimitives.h>
#include <Engine/EditorUI/Text/Font/Font.h>
#include <Engine/Utility/StringUtil/StringUtil.h>

namespace
{
	using namespace EditorUI;

	// -------------------------------------------------------------------------------
	// ウィジェットを描いてよい状態かを判定し、描画先のWindowFrameを返す
	//
	// すべてのウィジェットが先頭で同じ確認を行うため、ここに集約している
	//
	// @return	描画可能ならWindowFrame。BeginWindowの外や折り畳み中ならnullptr
	// -------------------------------------------------------------------------------
	WindowFrame* GetDrawableFrame(Context& _ctx)
	{
		WindowFrame* pFrame = _ctx.GetCurrentWindow();

		if (pFrame == nullptr || pFrame->SkipContents)
		{
			return nullptr;
		}

		return pFrame;
	}

	// -------------------------------------------------------------------------------
	// 組み上がった行を実際に描く
	//
	// TextLayoutが決めた「どこで折り返し、どこを省略するか」に従って
	// 1行ずつDrawListへ積むだけの処理
	// -------------------------------------------------------------------------------
	void DrawTextLines(
		WindowFrame&					_frame,
		Font&							_font,
		std::wstring_view				_text,
		const std::vector<TextLine>&	_lines,
		const Rect2D&					_bounds,
		const TextOptions&				_options,
		Color32							_color)
	{
		const float lineHeight		= _font.GetLineHeight() + _options.LineSpacing;
		const float availableWidth	= _bounds.Width();

		if (_options.ClipToBounds)
		{
			_frame.Draw.PushClipRect(_bounds);
		}

		float y = _bounds.Min.y;

		for (const TextLine& line : _lines)
		{
			const std::wstring_view lineText = _text.substr(line.Begin, line.End - line.Begin);

			const float offsetX = TextLayout::ResolveAlignmentOffsetX(
				_options.HorizontalAlignment, availableWidth, line.Width);

			const DirectX::XMFLOAT2 pos{ _bounds.Min.x + offsetX, y };

			_frame.Draw.AddText(pos, _color, lineText, _font);

			// 省略された行は、切り詰めた本文のすぐ後ろに省略記号を描き足す
			if (line.Ellipsis)
			{
				const float textWidth = TextLayout::MeasureWidth(_font, lineText);
				_frame.Draw.AddText(
					{ pos.x + textWidth, y }, _color, TextLayout::kEllipsis, _font);
			}

			y += lineHeight;
		}

		if (_options.ClipToBounds)
		{
			_frame.Draw.PopClipRect();
		}
	}

	// Text系の共通実装。UTF-8版とワイド文字版はどちらもここへ合流する
	TextMetrics TextInternal(
		Context&			_ctx,
		Font&				_font,
		std::wstring_view	_text,
		const TextOptions&	_options)
	{
		WindowFrame* pFrame = GetDrawableFrame(_ctx);
		if (pFrame == nullptr)
		{
			return {};
		}

		const Style& style = _ctx.GetStyle();

		// 折り返し幅の基準は、カーソル位置から行末までの残り幅
		const float availableWidth = (std::max)(
			0.0f,
			GetContentWidth(pFrame, style) - (pFrame->CursorPos.x - pFrame->ContentOrigin.x));

		std::vector<TextLine> lines;
		const TextMetrics metrics = TextLayout::Build(_font, _text, _options, availableWidth, lines);

		// 折り返す設定のときは、実際の文字幅ではなく領域幅を占有する
		// そうしないと行揃え(Center / Right)の基準幅が行ごとに変わってしまう
		const DirectX::XMFLOAT2 itemSize
		{
			_options.Wrap ? availableWidth : metrics.Size.x,
			metrics.Size.y
		};

		const Rect2D bounds	= PlaceWidget(pFrame, itemSize, style.ItemSpacing);
		const Color32 color	= _options.UseCustomColor ? _options.Color : style.ColorText;

		DrawTextLines(*pFrame, _font, _text, lines, bounds, _options, color);

		return metrics;
	}
}

// -------------------------------------------------------------------------------
// テキスト計測
// -------------------------------------------------------------------------------
EditorUI::TextMetrics EditorUI::MeasureText(Font& _font, std::string_view _utf8Text, const TextOptions& _options)
{
	return MeasureText(_font, StringUtil::Utf8ToWide(_utf8Text), _options);
}

EditorUI::TextMetrics EditorUI::MeasureText(Font& _font, std::wstring_view _text, const TextOptions& _options)
{
	std::vector<TextLine> lines;

	// Contextを持たない呼び出しなので、折り返し幅は_options.WrapWidthだけを見る
	return TextLayout::Build(_font, _text, _options, _options.WrapWidth, lines);
}

// -------------------------------------------------------------------------------
// テキスト描画
// -------------------------------------------------------------------------------
EditorUI::TextMetrics EditorUI::Text(Context& _ctx, Font& _font, std::string_view _utf8Text, const TextOptions& _options)
{
	// UTF-8のままでは1文字を1バイトとして扱ってしまい、日本語が崩れる
	// 描画も計測も文字単位で行うため、入り口でワイド文字へ変換する
	return TextInternal(_ctx, _font, StringUtil::Utf8ToWide(_utf8Text), _options);
}

EditorUI::TextMetrics EditorUI::Text(Context& _ctx, Font& _font, std::wstring_view _text, const TextOptions& _options)
{
	return TextInternal(_ctx, _font, _text, _options);
}

EditorUI::TextMetrics EditorUI::TextColored(Context& _ctx, Font& _font, std::string_view _utf8Text, Color32 _color)
{
	TextOptions options;
	options.UseCustomColor	= true;
	options.Color			= _color;

	return Text(_ctx, _font, _utf8Text, options);
}

EditorUI::TextMetrics EditorUI::TextWrapped(Context& _ctx, Font& _font, std::string_view _utf8Text, float _wrapWidth)
{
	TextOptions options;
	options.Wrap		= true;
	options.WrapWidth	= _wrapWidth;
	options.Ellipsis	= false;	// 折り返すので、省略する必要がない

	return Text(_ctx, _font, _utf8Text, options);
}

EditorUI::TextMetrics EditorUI::TextMuted(Context& _ctx, Font& _font, std::string_view _utf8Text)
{
	return TextColored(_ctx, _font, _utf8Text, _ctx.GetStyle().ColorTextMuted);
}

// -------------------------------------------------------------------------------
// ボタン
// -------------------------------------------------------------------------------
bool EditorUI::Button(Context& _ctx, std::string_view _label, Font& _font, DirectX::XMFLOAT2 _size)
{
	WindowFrame* pFrame = GetDrawableFrame(_ctx);
	if (pFrame == nullptr)
	{
		return false;	// 折り畳み中は何もしない
	}

	// ラベルからこのボタンのIdを決める
	// 同じラベルでも、属するウィンドウが違えばIdStackのスコープが違うため衝突しない
	const Id id = _ctx.GetIdStack().GetId(_label);

	const Style&		style		= _ctx.GetStyle();
	const std::wstring	wideLabel	= StringUtil::Utf8ToWide(_label);
	const float			textWidth	= TextLayout::MeasureWidth(_font, wideLabel);
	const float			textHeight	= _font.GetLineHeight();

	// 指定されたサイズより文字が大きければ、テキストが収まるまでボタンを広げる
	const DirectX::XMFLOAT2 actualSize
	{
		(std::max)(_size.x, textWidth  + style.FramePaddingX * 2.0f),
		(std::max)(_size.y, textHeight + style.FramePaddingY * 2.0f)
	};

	const Rect2D			bounds		= PlaceWidget(pFrame, actualSize, style.ItemSpacing);
	const InteractionState	interaction	= UpdateInteraction(_ctx, id, bounds);

	const Color32 color = interaction.Held		? style.ColorButtonActive
						: interaction.Hovered	? style.ColorButtonHovered
						: style.ColorButton;

	pFrame->Draw.AddRectFilled(bounds, color);
	pFrame->Draw.AddRectOutline(bounds, style.ColorBorder, style.BorderThickness);

	const DirectX::XMFLOAT2 textPos =
	{
		bounds.Min.x + (bounds.Width()  - textWidth)  * 0.5f,
		bounds.Min.y + (bounds.Height() - textHeight) * 0.5f
	};

	// ボタンの外へ文字がはみ出さないようにする
	pFrame->Draw.PushClipRect(bounds);
	pFrame->Draw.AddText(textPos, style.ColorText, wideLabel, _font);
	pFrame->Draw.PopClipRect();	// Pushと対にして戻す

	return interaction.Clicked;
}

// -------------------------------------------------------------------------------
// チェックボックス
// -------------------------------------------------------------------------------
bool EditorUI::Checkbox(Context& _ctx, std::string_view _label, bool* _pValue, float _boxSize)
{
	WindowFrame* pFrame = GetDrawableFrame(_ctx);
	if (pFrame == nullptr || _pValue == nullptr)
	{
		return false;
	}

	const Id id = _ctx.GetIdStack().GetId(_label);

	const Style&	style	= _ctx.GetStyle();
	const float		boxSize	= (std::max)(1.0f, _boxSize);
	const Rect2D	bounds	= PlaceWidget(pFrame, { boxSize, boxSize }, style.ItemSpacing);

	// 描画と判定の中身はProperty(bool*)と共通なので、プリミティブ側に任せる
	return CheckboxBehavior(_ctx, id, bounds, _pValue);
}

// -------------------------------------------------------------------------------
// セパレーター
// -------------------------------------------------------------------------------
void EditorUI::Separator(Context& _ctx)
{
	WindowFrame* pFrame = GetDrawableFrame(_ctx);
	if (pFrame == nullptr)
	{
		return;
	}

	const Style& style = _ctx.GetStyle();
	constexpr float kThickness = 1.0f;

	// pState->Sizeではなく、実際に描画中のWindowRectを使う
	// Dock中はWindowStateのSizeが最後のフローティング時のままで、実寸と一致しないため
	const float		availableWidth	= GetContentWidth(pFrame, style);
	const Rect2D	bounds			= PlaceWidget(pFrame, { availableWidth, kThickness }, style.ItemSpacing);

	pFrame->Draw.AddRectFilled(bounds, style.ColorBorder);
}

// -------------------------------------------------------------------------------
// レイアウト操作
// -------------------------------------------------------------------------------
void EditorUI::SameLine(Context& _ctx, float _spacing)
{
	WindowFrame* pFrame = _ctx.GetCurrentWindow();
	if (pFrame == nullptr)
	{
		return;
	}

	const float spacing = (_spacing > 0.0f) ? _spacing : _ctx.GetStyle().ItemSpacing;
	SameLine(pFrame, spacing);
}

void EditorUI::SetNextItemWidth(Context& _ctx, float _width)
{
	SetNextItemWidth(_ctx.GetCurrentWindow(), _width);
}
