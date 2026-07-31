// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Widgets.h"
#include <Engine/EditorUI/Widgets/Layout/Layout.h>
#include <Engine/EditorUI/Widgets/WidgetInteraction.h>
#include <Engine/EditorUI/Text/Font/Font.h>

// ボタン
bool EditorUI::Button(Context& _ctx, std::string_view _label, Font& _font, DirectX::XMFLOAT2 _size)
{
	WindowFrame* pFrame = _ctx.GetCurrentWindow();
	if (pFrame->SkipContents) 
	{ return false; }	// 折り畳み中は何もしない

	// ラベルからこのボタンのIDを決める。
	[[maybe_unused]] const Id id = _ctx.GetIdStack().GetId(_label);

	const Style& style = _ctx.GetStyle();
	const Rect2D bounds = PlaceWidget(pFrame, _size, style.ItemSpacing);
	const InteractionState interaction = UpdateInteraction(_ctx, bounds);

	const Color32 color = interaction.Held ? style.ColorButtonActive
						: interaction.Hovered ? style.ColorButtonHovered
						: style.ColorButton;

	pFrame->Draw.AddRectFilled(bounds, color);
	pFrame->Draw.AddRectOutline(bounds, style.ColorBorder, style.BorderThickness);

	// ラベルをボタン中央に描く
	std::wstring wlabel(_label.begin(), _label.end());
	const float textWidth = static_cast<float>(wlabel.size()) * (_font.GetLineHeight() * 0.5f);
	const DirectX::XMFLOAT2 textPos =
	{
		bounds.Min.x + (bounds.Width() - textWidth) * 0.5f,
		bounds.Min.y + (bounds.Height() - _font.GetLineHeight()) * 0.5f
	};
	pFrame->Draw.AddText(textPos, style.ColorText, wlabel, _font);

	return interaction.Clicked;
}

// チェックボックス
bool EditorUI::Checkbox(Context& _ctx, std::string_view _label, bool* _pValue, float _boxSize)
{
	WindowFrame* pFrame = _ctx.GetCurrentWindow();
	if (pFrame->SkipContents) 
	{ return false; }

	[[maybe_unused]] const Id id = _ctx.GetIdStack().GetId(_label);

	const Style& style = _ctx.GetStyle();
	const Rect2D bounds = PlaceWidget(pFrame, { _boxSize, _boxSize }, style.ItemSpacing);
	const InteractionState interaction = UpdateInteraction(_ctx, bounds);

	if (interaction.Clicked)
	{ *_pValue = !*_pValue; }	// 呼び出し元の値をこの場で直接トグルする

	const Color32 color = interaction.Hovered ? style.ColorButtonHovered : style.ColorButton;
	pFrame->Draw.AddRectFilled(bounds, color);
	pFrame->Draw.AddRectOutline(bounds, style.ColorBorder, style.BorderThickness);

	// チェック済みなら内側に一回り小さい矩形を重ねてチェック印の代用にする
	if (*_pValue)
	{
		const float inset = _boxSize * 0.25f;
		const Rect2D checkMark =
		{
			{bounds.Min.x + inset, bounds.Min.y + inset},
			{bounds.Max.x - inset, bounds.Max.y - inset}
		};
		pFrame->Draw.AddRectFilled(checkMark, style.ColorText);
	}

	return interaction.Clicked;
}

// セパレーター
void EditorUI::Separator(Context& _ctx)
{
	WindowFrame* pFrame = _ctx.GetCurrentWindow();
	if (pFrame->SkipContents)
	{ return; }

	const Style& style = _ctx.GetStyle();
	constexpr float thickness = 1.0f;

	const float width = pFrame->pState->Size.x - style.WindowPadding * 2.0f;
	const Rect2D bounds = PlaceWidget(pFrame, { width, thickness }, style.ItemSpacing);
	pFrame->Draw.AddRectFilled(bounds, style.ColorBorder);
}
