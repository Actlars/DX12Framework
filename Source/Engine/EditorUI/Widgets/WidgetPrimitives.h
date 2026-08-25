#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Widgets/WidgetInteraction.h>

// -------------------------------------------------------------------------------
// WidgetPrimitives
//
// 概要 :
//	「配置は済んでいて、この矩形に描くだけ」という単位に切り出した部品群
//
//	通常のウィジェット(Checkboxなど)は自分でレイアウトに1行を消費するが、
//	プロパティ行のように「ラベルの右側の決まった矩形に置きたい」場合は
//	レイアウトを介さずに描く必要がある
//	その2つの用途で処理が重複しないよう、共通部分をここに置く
// -------------------------------------------------------------------------------
namespace EditorUI
{
	// -------------------------------------------------------------------------------
	// @brief	枠線付きの背景を描く。入力欄・ボタンの土台として使う
	//
	// @param[in,out]	_frame		描画先
	// @param[in]		_bounds		描く矩形
	// @param[in]		_background	背景色
	// @param[in]		_style		枠線の色と太さの取得元
	// -------------------------------------------------------------------------------
	inline void DrawFrame(WindowFrame& _frame, const Rect2D& _bounds, Color32 _background, const Style& _style)
	{
		_frame.Draw.AddRectFilled(_bounds, _background);
		_frame.Draw.AddRectOutline(_bounds, _style.ColorBorder, _style.BorderThickness);
	}

	// -------------------------------------------------------------------------------
	// @brief	指定した矩形にチェックボックスを描き、クリックで値をトグルする
	//
	//	レイアウトには関与しないため、呼び出し側が矩形を決めてから使う
	//
	// @param[in]		_ctx		描画対象のContext
	// @param[in]		_id			このチェックボックスのId
	// @param[in]		_bounds		描く矩形
	// @param[in,out]	_pValue		チェック状態を持つ変数へのポインタ
	// @return	true : このフレームで値がトグルされた / false : それ以外
	// -------------------------------------------------------------------------------
	inline bool CheckboxBehavior(Context& _ctx, Id _id, const Rect2D& _bounds, bool* _pValue)
	{
		if (_pValue == nullptr)
		{
			return false;
		}

		WindowFrame* pFrame = _ctx.GetCurrentWindow();
		if (pFrame == nullptr)
		{
			return false;
		}

		const Style&			style		= _ctx.GetStyle();
		const InteractionState	interaction	= UpdateInteraction(_ctx, _id, _bounds);

		if (interaction.Clicked)
		{
			*_pValue = !*_pValue;	// 呼び出し元の値をこの場で直接トグルする
		}

		const Color32 background = interaction.Held		? style.ColorButtonActive
								 : interaction.Hovered	? style.ColorButtonHovered
								 : style.ColorButton;

		DrawFrame(*pFrame, _bounds, background, style);

		// チェック済みなら内側に一回り小さい矩形を重ねてチェック印の代用にする
		if (*_pValue)
		{
			const float inset = (std::min)(_bounds.Width(), _bounds.Height()) * 0.25f;
			const Rect2D checkMark =
			{
				{ _bounds.Min.x + inset, _bounds.Min.y + inset },
				{ _bounds.Max.x - inset, _bounds.Max.y - inset }
			};
			pFrame->Draw.AddRectFilled(checkMark, style.ColorText);
		}

		return interaction.Clicked;
	}
}
