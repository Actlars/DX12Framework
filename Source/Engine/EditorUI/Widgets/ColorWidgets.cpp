// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Widgets.h"
#include <Engine/EditorUI/Widgets/Layout/Layout.h>
#include <Engine/EditorUI/Widgets/WidgetInteraction.h>
#include <Engine/EditorUI/Text/Font/Font.h>
#include <Engine/Utility/StringUtil/StringUtil.h>

// -------------------------------------------------------------------------------
// ColorWidgets
//
// 概要 :
//	色を選ぶためのウィジェット群
//
//	数値の入力欄だけでは「どんな色になるか」が想像しにくいため、
//	色相環（円）から直感的に選べるピッカーを用意する
//
//	構成
//		ColorButton		現在の色を示す四角。押すとピッカーを開く
//		ColorPicker4	色相環 + 明度スライダ + 不透明度スライダ + 数値欄
//		ColorProperty	「ラベル + 色見本」の1行。押すとピッカーがポップアップで開く
//
//	色の持ち方
//		呼び出し側は常に RGBA(0.0～1.0) で持つ
//		HSV への変換はこのファイルの中だけで完結させ、
//		アプリ側に色空間の知識を持ち込ませない
// -------------------------------------------------------------------------------
namespace
{
	using namespace EditorUI;

	constexpr float kPi = 3.14159265358979323846f;

	// 色相環の分割数
	// 多いほどなめらかになるが、そのぶん頂点が増える
	// 64もあれば、階段状の縁は肉眼では分からない
	constexpr int kWheelSegments = 64;

	// -------------------------------------------------------------------------------
	// ウィジェットを描いてよい状態かを判定し、描画先のWindowFrameを返す
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
	// HSV → RGB
	//
	//	_hue		0.0～1.0（1.0で一周）
	//	_saturation	0.0(白) ～ 1.0(純色)
	//	_value		0.0(黒) ～ 1.0(明るい)
	// -------------------------------------------------------------------------------
	DirectX::XMFLOAT3 HsvToRgb(float _hue, float _saturation, float _value)
	{
		// 色相を6分割し、どの区間にいるかで組み立てる
		const float h = (_hue - std::floor(_hue)) * 6.0f;
		const int   i = static_cast<int>(h);
		const float f = h - static_cast<float>(i);

		const float p = _value * (1.0f - _saturation);
		const float q = _value * (1.0f - _saturation * f);
		const float t = _value * (1.0f - _saturation * (1.0f - f));

		switch (i)
		{
		case 0:  return { _value, t, p };
		case 1:  return { q, _value, p };
		case 2:  return { p, _value, t };
		case 3:  return { p, q, _value };
		case 4:  return { t, p, _value };
		default: return { _value, p, q };
		}
	}

	// -------------------------------------------------------------------------------
	// RGB → HSV
	//
	//	彩度0（無彩色）のときは色相が決まらない
	//	そのため呼び出し側は、直前の色相を渡して引き継げるようにしている
	// -------------------------------------------------------------------------------
	void RgbToHsv(
		const DirectX::XMFLOAT3&	_rgb,
		float						_fallbackHue,
		float&						_outHue,
		float&						_outSaturation,
		float&						_outValue)
	{
		const float maxValue = (std::max)({ _rgb.x, _rgb.y, _rgb.z });
		const float minValue = (std::min)({ _rgb.x, _rgb.y, _rgb.z });
		const float delta    = maxValue - minValue;

		_outValue      = maxValue;
		_outSaturation = (maxValue > 0.0f) ? (delta / maxValue) : 0.0f;

		if (delta <= 0.0001f)
		{
			// 無彩色。色相は決まらないので、直前の値をそのまま使う
			_outHue = _fallbackHue;
			return;
		}

		float hue = 0.0f;

		if (maxValue == _rgb.x)		{ hue = (_rgb.y - _rgb.z) / delta; }
		else if (maxValue == _rgb.y){ hue = 2.0f + (_rgb.z - _rgb.x) / delta; }
		else						{ hue = 4.0f + (_rgb.x - _rgb.y) / delta; }

		hue /= 6.0f;
		_outHue = hue - std::floor(hue);
	}

	// -------------------------------------------------------------------------------
	// 市松模様を敷く
	//
	//	半透明の色をそのまま置くと、背景の濃さによって見え方が変わってしまう
	//	下地に市松模様を敷くことで、どのくらい透けているかが一目で分かる
	// -------------------------------------------------------------------------------
	void DrawCheckerboard(WindowFrame& _frame, const Rect2D& _bounds, float _cellSize)
	{
		const Color32 light = MakeColor(120, 120, 124);
		const Color32 dark  = MakeColor( 78,  78,  82);

		_frame.Draw.PushClipRect(_bounds);

		int row = 0;
		for (float y = _bounds.Min.y; y < _bounds.Max.y; y += _cellSize, ++row)
		{
			int column = 0;
			for (float x = _bounds.Min.x; x < _bounds.Max.x; x += _cellSize, ++column)
			{
				_frame.Draw.AddRectFilled(
					{ { x, y }, { x + _cellSize, y + _cellSize } },
					((row + column) % 2 == 0) ? light : dark);
			}
		}

		_frame.Draw.PopClipRect();
	}

	// -------------------------------------------------------------------------------
	// 色相環を描く
	//
	//	中心（彩度0）から外周（彩度1）へ向かって色が濃くなる扇形を並べる
	//	明度は現在値を反映させ、「いま選んでいる明るさでの色」が見えるようにする
	// -------------------------------------------------------------------------------
	void DrawColorWheel(
		WindowFrame&				_frame,
		const DirectX::XMFLOAT2&	_center,
		float						_radius,
		float						_value)
	{
		const DirectX::XMFLOAT3 centerRgb = HsvToRgb(0.0f, 0.0f, _value);
		const Color32 centerColor = MakeColor(
			static_cast<uint32_t>(centerRgb.x * 255.0f),
			static_cast<uint32_t>(centerRgb.y * 255.0f),
			static_cast<uint32_t>(centerRgb.z * 255.0f));

		for (int i = 0; i < kWheelSegments; ++i)
		{
			const float hue0 = static_cast<float>(i)     / static_cast<float>(kWheelSegments);
			const float hue1 = static_cast<float>(i + 1) / static_cast<float>(kWheelSegments);

			// 画面のY軸は下向きなので、角度もそれに合わせる
			const float angle0 = hue0 * kPi * 2.0f;
			const float angle1 = hue1 * kPi * 2.0f;

			const DirectX::XMFLOAT2 outer0
			{
				_center.x + std::cos(angle0) * _radius,
				_center.y + std::sin(angle0) * _radius
			};
			const DirectX::XMFLOAT2 outer1
			{
				_center.x + std::cos(angle1) * _radius,
				_center.y + std::sin(angle1) * _radius
			};

			const DirectX::XMFLOAT3 rgb0 = HsvToRgb(hue0, 1.0f, _value);
			const DirectX::XMFLOAT3 rgb1 = HsvToRgb(hue1, 1.0f, _value);

			const Color32 color0 = MakeColor(
				static_cast<uint32_t>(rgb0.x * 255.0f),
				static_cast<uint32_t>(rgb0.y * 255.0f),
				static_cast<uint32_t>(rgb0.z * 255.0f));
			const Color32 color1 = MakeColor(
				static_cast<uint32_t>(rgb1.x * 255.0f),
				static_cast<uint32_t>(rgb1.y * 255.0f),
				static_cast<uint32_t>(rgb1.z * 255.0f));

			_frame.Draw.AddTriangleGradient(
				_center, centerColor,
				outer0,  color0,
				outer1,  color1);
		}
	}

	// -------------------------------------------------------------------------------
	// 縦方向のグラデーション帯を描く（明度・不透明度のスライダ用）
	// -------------------------------------------------------------------------------
	void DrawVerticalGradient(
		WindowFrame&	_frame,
		const Rect2D&	_bounds,
		Color32			_topColor,
		Color32			_bottomColor)
	{
		// 上下2色の四角形。三角形2枚に分けて頂点カラーで補間する
		_frame.Draw.AddTriangleGradient(
			{ _bounds.Min.x, _bounds.Min.y }, _topColor,
			{ _bounds.Max.x, _bounds.Min.y }, _topColor,
			{ _bounds.Max.x, _bounds.Max.y }, _bottomColor);

		_frame.Draw.AddTriangleGradient(
			{ _bounds.Min.x, _bounds.Min.y }, _topColor,
			{ _bounds.Max.x, _bounds.Max.y }, _bottomColor,
			{ _bounds.Min.x, _bounds.Max.y }, _bottomColor);
	}

	// -------------------------------------------------------------------------------
	// スライダの現在位置に横線のつまみを描く
	// -------------------------------------------------------------------------------
	void DrawSliderMarker(WindowFrame& _frame, const Rect2D& _bounds, float _t, const Style& _style)
	{
		const float y = _bounds.Min.y + _bounds.Height() * std::clamp(_t, 0.0f, 1.0f);

		// 明るい線と暗い線を重ね、どんな色の上でも見えるようにする
		_frame.Draw.AddRectFilled(
			{ { _bounds.Min.x - 2.0f, y - 2.0f }, { _bounds.Max.x + 2.0f, y + 2.0f } },
			_style.ColorBorder);

		_frame.Draw.AddRectFilled(
			{ { _bounds.Min.x - 2.0f, y - 1.0f }, { _bounds.Max.x + 2.0f, y + 1.0f } },
			_style.ColorTextBright);
	}

	// XMFLOAT4(0.0～1.0) を頂点カラーへ
	Color32 ToColor32(const DirectX::XMFLOAT4& _color)
	{
		const auto toByte = [](float _value)
		{
			return static_cast<uint32_t>(std::clamp(_value, 0.0f, 1.0f) * 255.0f + 0.5f);
		};

		return MakeColor(toByte(_color.x), toByte(_color.y), toByte(_color.z), toByte(_color.w));
	}
}

// -------------------------------------------------------------------------------
// 色見本のボタン
// -------------------------------------------------------------------------------
bool EditorUI::ColorButton(
	Context&					_ctx,
	std::string_view			_idLabel,
	const DirectX::XMFLOAT4&	_color,
	const DirectX::XMFLOAT2&	_size)
{
	WindowFrame* pFrame = GetDrawableFrame(_ctx);
	if (pFrame == nullptr)
	{
		return false;
	}

	const Style&	style	= _ctx.GetStyle();
	const Id		id		= _ctx.GetIdStack().GetId(_idLabel);

	const DirectX::XMFLOAT2 size
	{
		(_size.x > 0.0f) ? _size.x : GetContentWidth(pFrame, style),
		(_size.y > 0.0f) ? _size.y : 20.0f
	};

	const Rect2D			bounds		= PlaceWidget(pFrame, size, style.ItemSpacing);
	const InteractionState	interaction	= UpdateInteraction(_ctx, id, bounds);

	// 半透明でも「どのくらい透けているか」が分かるよう、下地に市松模様を敷く
	DrawCheckerboard(*pFrame, bounds, 6.0f);

	pFrame->Draw.AddRectFilled(bounds, ToColor32(_color));

	pFrame->Draw.AddRectOutline(
		bounds,
		interaction.Hovered ? style.ColorTextBright : style.ColorBorder,
		style.BorderThickness);

	return interaction.Clicked;
}

// -------------------------------------------------------------------------------
// 色相環によるカラーピッカー
//
// 画面構成
//	左   : 色相環（角度 = 色相、中心からの距離 = 彩度）
//	右   : 明度スライダ、不透明度スライダ
//	下   : 現在の色と、RGBAの数値欄
// -------------------------------------------------------------------------------
bool EditorUI::ColorPicker4(
	Context&			_ctx,
	Font&				_font,
	std::string_view	_idLabel,
	DirectX::XMFLOAT4*	_color)
{
	WindowFrame* pFrame = GetDrawableFrame(_ctx);
	if (pFrame == nullptr || _color == nullptr)
	{
		return false;
	}

	const Style&	style	= _ctx.GetStyle();
	const Id		baseId	= _ctx.GetIdStack().GetId(_idLabel);

	bool changed = false;

	// -------------------------------------------------------------------------------
	// 現在の色をHSVへ分解する
	//
	//	彩度0のときは色相が決まらないため、前回の色相を引き継ぐ
	//	そうしないと、白に近づけた瞬間に色相が0へ飛んでしまう
	// -------------------------------------------------------------------------------
	const float previousHue = _ctx.GetStorageFloat(baseId, 0.0f);

	float hue = 0.0f;
	float saturation = 0.0f;
	float value = 0.0f;

	RgbToHsv({ _color->x, _color->y, _color->z }, previousHue, hue, saturation, value);

	// -------------------------------------------------------------------------------
	// レイアウト
	// -------------------------------------------------------------------------------
	constexpr float kSliderWidth	= 18.0f;
	constexpr float kSliderGap		= 10.0f;

	const float availableWidth	= GetContentWidth(pFrame, style);
	const float wheelDiameter	= (std::max)(
		80.0f,
		(std::min)(availableWidth - (kSliderWidth + kSliderGap) * 2.0f, 180.0f));

	const Rect2D area = Dummy(_ctx, { availableWidth, wheelDiameter });
	if (!area.IsValid())
	{
		return false;
	}

	const float radius = wheelDiameter * 0.5f;

	const DirectX::XMFLOAT2 center
	{
		area.Min.x + radius,
		area.Min.y + radius
	};

	const Rect2D valueBar = MakeRect(
		{ center.x + radius + kSliderGap, area.Min.y },
		{ kSliderWidth, wheelDiameter });

	const Rect2D alphaBar = MakeRect(
		{ valueBar.Max.x + kSliderGap, area.Min.y },
		{ kSliderWidth, wheelDiameter });

	// -------------------------------------------------------------------------------
	// 色相環
	// -------------------------------------------------------------------------------
	DrawColorWheel(*pFrame, center, radius, (std::max)(value, 0.05f));

	{
		const Id wheelId = _ctx.GetIdStack().GetId("##wheel");

		// 円の当たり判定は、外接する正方形で受けてから半径で絞る
		const Rect2D wheelBounds = MakeRect(
			{ center.x - radius, center.y - radius }, { wheelDiameter, wheelDiameter });

		const InteractionState interaction = UpdateInteraction(_ctx, wheelId, wheelBounds);

		if (interaction.Held)
		{
			const DirectX::XMFLOAT2 mouse = _ctx.GetMousePos();

			const float dx = mouse.x - center.x;
			const float dy = mouse.y - center.y;
			const float distance = std::sqrt(dx * dx + dy * dy);

			// 円の外まで引っ張っても操作を続けられるよう、彩度は1.0で頭打ちにする
			saturation = std::clamp(distance / radius, 0.0f, 1.0f);

			if (distance > 0.0001f)
			{
				float angle = std::atan2(dy, dx);
				if (angle < 0.0f)
				{
					angle += kPi * 2.0f;
				}
				hue = angle / (kPi * 2.0f);
			}

			changed = true;
		}
	}

	// 選択位置の印
	{
		const float angle = hue * kPi * 2.0f;
		const DirectX::XMFLOAT2 marker
		{
			center.x + std::cos(angle) * radius * saturation,
			center.y + std::sin(angle) * radius * saturation
		};

		// 白と黒の二重丸（四角で近似）にして、どんな色の上でも見えるようにする
		pFrame->Draw.AddRectOutline(
			{ { marker.x - 5.0f, marker.y - 5.0f }, { marker.x + 5.0f, marker.y + 5.0f } },
			style.ColorBorder, 1.0f);

		pFrame->Draw.AddRectOutline(
			{ { marker.x - 4.0f, marker.y - 4.0f }, { marker.x + 4.0f, marker.y + 4.0f } },
			style.ColorTextBright, 1.0f);
	}

	// -------------------------------------------------------------------------------
	// 明度スライダ（上が明るく、下が黒）
	// -------------------------------------------------------------------------------
	{
		const DirectX::XMFLOAT3 topRgb = HsvToRgb(hue, saturation, 1.0f);

		DrawVerticalGradient(
			*pFrame, valueBar,
			MakeColor(
				static_cast<uint32_t>(topRgb.x * 255.0f),
				static_cast<uint32_t>(topRgb.y * 255.0f),
				static_cast<uint32_t>(topRgb.z * 255.0f)),
			MakeColor(0, 0, 0));

		pFrame->Draw.AddRectOutline(valueBar, style.ColorBorder, style.BorderThickness);

		const Id valueId = _ctx.GetIdStack().GetId("##value");
		const InteractionState interaction = UpdateInteraction(_ctx, valueId, valueBar);

		if (interaction.Held)
		{
			const float t = (_ctx.GetMousePos().y - valueBar.Min.y) / (std::max)(1.0f, valueBar.Height());
			value = 1.0f - std::clamp(t, 0.0f, 1.0f);
			changed = true;
		}

		DrawSliderMarker(*pFrame, valueBar, 1.0f - value, style);
	}

	// -------------------------------------------------------------------------------
	// 不透明度スライダ（上が不透明、下が透明）
	// -------------------------------------------------------------------------------
	{
		DrawCheckerboard(*pFrame, alphaBar, 6.0f);

		const DirectX::XMFLOAT3 rgb = HsvToRgb(hue, saturation, value);

		const Color32 opaque = MakeColor(
			static_cast<uint32_t>(rgb.x * 255.0f),
			static_cast<uint32_t>(rgb.y * 255.0f),
			static_cast<uint32_t>(rgb.z * 255.0f), 255);

		DrawVerticalGradient(*pFrame, alphaBar, opaque, WithAlpha(opaque, 0));

		pFrame->Draw.AddRectOutline(alphaBar, style.ColorBorder, style.BorderThickness);

		const Id alphaId = _ctx.GetIdStack().GetId("##alpha");
		const InteractionState interaction = UpdateInteraction(_ctx, alphaId, alphaBar);

		if (interaction.Held)
		{
			const float t = (_ctx.GetMousePos().y - alphaBar.Min.y) / (std::max)(1.0f, alphaBar.Height());
			_color->w = 1.0f - std::clamp(t, 0.0f, 1.0f);
			changed = true;
		}

		DrawSliderMarker(*pFrame, alphaBar, 1.0f - _color->w, style);
	}

	// -------------------------------------------------------------------------------
	// 円やスライダを操作した結果をRGBへ戻す
	// -------------------------------------------------------------------------------
	if (changed)
	{
		const DirectX::XMFLOAT3 rgb = HsvToRgb(hue, saturation, value);

		_color->x = rgb.x;
		_color->y = rgb.y;
		_color->z = rgb.z;
	}

	// 次回のために色相を覚えておく
	_ctx.SetStorageFloat(baseId, hue);

	// -------------------------------------------------------------------------------
	// 数値でも指定できるようにする
	//
	//	環では出しにくい正確な値（例 : R=0.5 ちょうど）を入れたい場面があるため、
	//	どちらからでも編集できるようにしておく
	// -------------------------------------------------------------------------------
	NumericEditorOptions<float> options;
	options.Min			= 0.0f;
	options.Max			= 1.0f;
	options.DragSpeed	= 0.005L;
	options.Precision	= 3;
	options.Step		= 0.0f;

	constexpr PropertyLayout kLayout{ 24.0f, 0.0f, 4.0f };

	changed |= Property(_ctx, _font, "R", &_color->x, options, kLayout);
	changed |= Property(_ctx, _font, "G", &_color->y, options, kLayout);
	changed |= Property(_ctx, _font, "B", &_color->z, options, kLayout);
	changed |= Property(_ctx, _font, "A", &_color->w, options, kLayout);

	return changed;
}

// -------------------------------------------------------------------------------
// 「ラベル + 色見本」の1行
//
// 見本を押すとカラーピッカーがポップアップで開く
// インスペクタの行を1行に保ちつつ、必要なときだけ大きなピッカーを出せる
// -------------------------------------------------------------------------------
bool EditorUI::ColorProperty(
	Context&				_ctx,
	Font&					_font,
	std::string_view		_label,
	DirectX::XMFLOAT4*		_color,
	const PropertyLayout&	_layout)
{
	WindowFrame* pFrame = GetDrawableFrame(_ctx);
	if (pFrame == nullptr || _color == nullptr)
	{
		return false;
	}

	const Style& style = _ctx.GetStyle();

	// 同じラベルが他の行と衝突しないよう、この行のスコープを作る
	_ctx.GetIdStack().PushString(_label);

	const float rowHeight = (_layout.RowHeight > 0.0f)
		? _layout.RowHeight
		: _font.GetLineHeight() + style.FramePaddingY * 2.0f;

	// -------------------------------------------------------------------------------
	// ラベル
	// -------------------------------------------------------------------------------
	{
		const Rect2D labelBounds = PlaceWidget(pFrame, { _layout.LabelWidth, rowHeight }, 0.0f);

		const DirectX::XMFLOAT2 textPos
		{
			labelBounds.Min.x,
			labelBounds.Min.y + (rowHeight - _font.GetLineHeight()) * 0.5f
		};

		pFrame->Draw.PushClipRect(labelBounds);
		pFrame->Draw.AddText(textPos, style.ColorText, StringUtil::Utf8ToWide(_label), _font);
		pFrame->Draw.PopClipRect();
	}

	SameLine(_ctx, style.ItemInnerSpacing);

	// -------------------------------------------------------------------------------
	// 色見本。押すとピッカーを開く
	// -------------------------------------------------------------------------------
	const float swatchWidth = (std::max)(
		40.0f,
		GetContentWidth(pFrame, style) - _layout.LabelWidth - style.ItemInnerSpacing);

	if (ColorButton(_ctx, "##swatch", *_color, { swatchWidth, rowHeight }))
	{
		_ctx.OpenPopup("##colorpicker");
	}

	bool changed = false;

	if (_ctx.BeginPopup("##colorpicker"))
	{
		changed = ColorPicker4(_ctx, _font, "##picker", _color);
		_ctx.EndPopup();
	}

	_ctx.GetIdStack().Pop();

	return changed;
}
