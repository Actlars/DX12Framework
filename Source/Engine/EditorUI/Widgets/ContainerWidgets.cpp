// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Widgets.h"
#include <Engine/EditorUI/Widgets/Layout/Layout.h>
#include <Engine/EditorUI/Widgets/WidgetInteraction.h>
#include <Engine/EditorUI/Widgets/WidgetPrimitives.h>
#include <Engine/EditorUI/Text/Font/Font.h>
#include <Engine/Utility/StringUtil/StringUtil.h>

// -------------------------------------------------------------------------------
// ContainerWidgets
//
// 概要 :
//	「行」「ツリー」「メニュー」「画像」といった、一覧表示のための部品群
//
//	Widgets.cppが1つの値を編集する部品を担当するのに対し、
//	こちらは複数の要素を並べて選ぶためのUIを担当する
//	どちらもレイアウト(Layout.h)と当たり判定(WidgetInteraction.h)の上に乗るため、
//	スクロールやクリップの扱いは自動的に本体と一致する
// -------------------------------------------------------------------------------
namespace
{
	using namespace EditorUI;

	// -------------------------------------------------------------------------------
	// ウィジェットを描いてよい状態かを判定し、描画先のWindowFrameを返す
	// Widgets.cppと同じ前提チェックだが、翻訳単位が違うためここにも用意する
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

	// 1行の標準的な高さ。フォントの行の高さに上下の余白を足したもの
	float GetRowHeight(Font& _font, const Style& _style, float _requestedHeight)
	{
		if (_requestedHeight > 0.0f)
		{
			return _requestedHeight;
		}
		return _font.GetLineHeight() + _style.RowPaddingY * 2.0f;
	}

	// -------------------------------------------------------------------------------
	// 行の中に、縦中央そろえで文字を1行描く
	// はみ出した分はクリップで隠す。行の幅は呼び出し側が決める
	// -------------------------------------------------------------------------------
	void DrawRowLabel(
		WindowFrame&		_frame,
		Font&				_font,
		const Rect2D&		_bounds,
		float				_textOffsetX,
		std::wstring_view	_text,
		Color32				_color)
	{
		const DirectX::XMFLOAT2 textPos
		{
			_bounds.Min.x + _textOffsetX,
			_bounds.Min.y + (_bounds.Height() - _font.GetLineHeight()) * 0.5f
		};

		// PushClipRectは親クリップとの積を取るため、
		// 行からも、ウィンドウからも文字がはみ出すことはない
		_frame.Draw.PushClipRect(_bounds);
		_frame.Draw.AddText(textPos, _color, _text, _font);
		_frame.Draw.PopClipRect();
	}

	// -------------------------------------------------------------------------------
	// 行の背景を、ホバー / 選択の状態に応じて塗る
	// -------------------------------------------------------------------------------
	void DrawRowBackground(
		WindowFrame&	_frame,
		const Style&	_style,
		const Rect2D&	_bounds,
		bool			_hovered,
		bool			_selected)
	{
		if (_selected)
		{
			_frame.Draw.AddRectFilled(_bounds, _style.ColorItemSelected);
		}
		else if (_hovered)
		{
			_frame.Draw.AddRectFilled(_bounds, _style.ColorItemHovered);
		}
	}

	// -------------------------------------------------------------------------------
	// 行に対する左右クリックの判定
	//
	// UpdateInteractionは左ボタン専用なので、右クリックはここで足す
	// 右クリックで入力を占有すると、そのままメニューを開けなくなるため
	// 「ホバー中に押された」だけを見る
	// -------------------------------------------------------------------------------
	ItemInteraction UpdateRowInteraction(Context& _ctx, Id _id, const Rect2D& _bounds)
	{
		const InteractionState state = UpdateInteraction(_ctx, _id, _bounds);

		ItemInteraction result;
		result.Hovered	= state.Hovered;
		result.Clicked	= state.Clicked;

		if (state.Hovered)
		{
			result.RightClicked		= _ctx.IsMouseClicked(MouseButton::Mouse_Right);
			result.DoubleClicked	= _ctx.IsMouseDoubleClicked(MouseButton::Mouse_Left);
		}

		return result;
	}

	// -------------------------------------------------------------------------------
	// ツリーの開閉を示す三角形を描く
	// 閉じているときは右向き、開いているときは下向き
	// -------------------------------------------------------------------------------
	void DrawTreeArrow(WindowFrame& _frame, const Rect2D& _bounds, bool _open, Color32 _color)
	{
		const DirectX::XMFLOAT2 center = _bounds.Center();
		const float half = _bounds.Width() * 0.5f;

		if (_open)
		{
			// 下向き
			_frame.Draw.AddTriangleFilled(
				{ center.x - half, center.y - half * 0.5f },
				{ center.x + half, center.y - half * 0.5f },
				{ center.x,        center.y + half * 0.7f },
				_color);
		}
		else
		{
			// 右向き
			_frame.Draw.AddTriangleFilled(
				{ center.x - half * 0.5f, center.y - half },
				{ center.x + half * 0.7f, center.y },
				{ center.x - half * 0.5f, center.y + half },
				_color);
		}
	}
}

// -------------------------------------------------------------------------------
// インデント
// -------------------------------------------------------------------------------
void EditorUI::Indent(Context& _ctx, float _width)
{
	WindowFrame* pFrame = _ctx.GetCurrentWindow();
	if (pFrame == nullptr) { return; }

	const float width = (_width > 0.0f) ? _width : _ctx.GetStyle().TreeIndentWidth;
	Indent(pFrame, width);
}

void EditorUI::Unindent(Context& _ctx, float _width)
{
	WindowFrame* pFrame = _ctx.GetCurrentWindow();
	if (pFrame == nullptr) { return; }

	const float width = (_width > 0.0f) ? _width : _ctx.GetStyle().TreeIndentWidth;
	Unindent(pFrame, width);
}

// -------------------------------------------------------------------------------
// 選択可能な1行
// -------------------------------------------------------------------------------
EditorUI::ItemInteraction EditorUI::Selectable(
	Context&			_ctx,
	Font&				_font,
	std::string_view	_label,
	bool				_selected,
	float				_height)
{
	WindowFrame* pFrame = GetDrawableFrame(_ctx);
	if (pFrame == nullptr)
	{
		return {};
	}

	const Style&	style	= _ctx.GetStyle();
	const Id		id		= _ctx.GetIdStack().GetId(_label);

	// 行は常にコンテンツ幅いっぱいに広げる
	// 幅が文字数で変わると、どこまでがクリック範囲なのかが分かりにくくなるため
	const DirectX::XMFLOAT2 size
	{
		GetContentWidth(pFrame, style),
		GetRowHeight(_font, style, _height)
	};

	const Rect2D			bounds		= PlaceWidget(pFrame, size, 0.0f);
	const ItemInteraction	interaction	= UpdateRowInteraction(_ctx, id, bounds);

	DrawRowBackground(*pFrame, style, bounds, interaction.Hovered, _selected);

	DrawRowLabel(
		*pFrame, _font, bounds, style.FramePaddingX,
		StringUtil::Utf8ToWide(_label),
		_selected ? style.ColorTextBright : style.ColorText);

	return interaction;
}

// -------------------------------------------------------------------------------
// ツリーの1ノード
//
// 開閉状態はContextのストレージへIdをキーに預けるため、
// 呼び出し側はツリー構造を描くことだけに集中できる
// -------------------------------------------------------------------------------
EditorUI::TreeNodeResult EditorUI::TreeNode(
	Context&				_ctx,
	Font&					_font,
	std::string_view		_label,
	const TreeNodeOptions&	_options)
{
	TreeNodeResult result;

	WindowFrame* pFrame = GetDrawableFrame(_ctx);
	if (pFrame == nullptr)
	{
		return result;
	}

	const Style&	style	= _ctx.GetStyle();
	const Id		id		= _ctx.GetIdStack().GetId(_label);

	// 葉ノードは常に閉じた扱い。開閉状態も持たせない
	const bool open = !_options.IsLeaf && _ctx.GetStorageBool(id, _options.DefaultOpen);

	const DirectX::XMFLOAT2 size
	{
		GetContentWidth(pFrame, style),
		GetRowHeight(_font, style, 0.0f)
	};

	const Rect2D bounds = PlaceWidget(pFrame, size, 0.0f);

	// 三角形の領域。ここを押したときだけ開閉し、行の他の場所は選択に使う
	const Rect2D arrowRect = MakeRect(
		{
			bounds.Min.x + style.FramePaddingX * 0.5f,
			bounds.Min.y + (bounds.Height() - style.TreeArrowSize) * 0.5f
		},
		{ style.TreeArrowSize, style.TreeArrowSize });

	const ItemInteraction interaction = UpdateRowInteraction(_ctx, id, bounds);

	DrawRowBackground(*pFrame, style, bounds, interaction.Hovered, _options.Selected);

	if (!_options.IsLeaf)
	{
		DrawTreeArrow(*pFrame, arrowRect, open,
			interaction.Hovered ? style.ColorTextBright : style.ColorTextMuted);
	}

	DrawRowLabel(
		*pFrame, _font, bounds,
		style.FramePaddingX * 0.5f + style.TreeArrowSize + style.ItemInnerSpacing,
		StringUtil::Utf8ToWide(_label),
		_options.Selected ? style.ColorTextBright : style.ColorText);

	// -------------------------------------------------------------------------------
	// 開閉の切り替え
	//	三角形の上をクリック	… 開閉のみ（選択は変えない）
	//	行のダブルクリック		… 開閉（フォルダを開く感覚に合わせる）
	// -------------------------------------------------------------------------------
	bool newOpen = open;

	if (!_options.IsLeaf)
	{
		const bool arrowClicked = interaction.Clicked &&
			arrowRect.Expanded(2.0f).Contains(_ctx.GetActiveIdClickPos());

		if (arrowClicked || interaction.DoubleClicked)
		{
			newOpen = !open;
			_ctx.SetStorageBool(id, newOpen);
		}
	}

	result.Open			= newOpen;
	result.Interaction	= interaction;

	// 開いている間は、子の行が1段深く並ぶようにインデントする
	if (result.Open)
	{
		Indent(pFrame, style.TreeIndentWidth);

		// 子のIdが親のスコープに属するようにしておく
		// 別のノードに同じ名前の子があってもIdが衝突しない
		_ctx.GetIdStack().PushString(_label);
	}

	return result;
}

void EditorUI::TreePop(Context& _ctx)
{
	WindowFrame* pFrame = _ctx.GetCurrentWindow();
	if (pFrame == nullptr) { return; }

	_ctx.GetIdStack().Pop();
	Unindent(pFrame, _ctx.GetStyle().TreeIndentWidth);
}

// -------------------------------------------------------------------------------
// 折り畳める見出し
// -------------------------------------------------------------------------------
bool EditorUI::CollapsingHeader(
	Context&			_ctx,
	Font&				_font,
	std::string_view	_label,
	bool				_defaultOpen)
{
	WindowFrame* pFrame = GetDrawableFrame(_ctx);
	if (pFrame == nullptr)
	{
		return false;
	}

	const Style&	style	= _ctx.GetStyle();
	const Id		id		= _ctx.GetIdStack().GetId(_label);

	const bool open = _ctx.GetStorageBool(id, _defaultOpen);

	const DirectX::XMFLOAT2 size
	{
		GetContentWidth(pFrame, style),
		GetRowHeight(_font, style, 0.0f)
	};

	const Rect2D			bounds		= PlaceWidget(pFrame, size, style.ItemSpacing);
	const InteractionState	interaction	= UpdateInteraction(_ctx, id, bounds);

	// 見出しは背景を常に塗り、セクションの境目が一目で分かるようにする
	pFrame->Draw.AddRectFilled(
		bounds,
		interaction.Hovered ? style.ColorItemHovered : style.ColorHeader);

	const Rect2D arrowRect = MakeRect(
		{
			bounds.Min.x + style.FramePaddingX * 0.5f,
			bounds.Min.y + (bounds.Height() - style.TreeArrowSize) * 0.5f
		},
		{ style.TreeArrowSize, style.TreeArrowSize });

	DrawTreeArrow(*pFrame, arrowRect, open, style.ColorTextBright);

	DrawRowLabel(
		*pFrame, _font, bounds,
		style.FramePaddingX * 0.5f + style.TreeArrowSize + style.ItemInnerSpacing,
		StringUtil::Utf8ToWide(_label),
		style.ColorTextBright);

	// 見出しは行のどこを押しても開閉する
	if (interaction.Clicked)
	{
		_ctx.SetStorageBool(id, !open);
		return !open;
	}

	return open;
}

// -------------------------------------------------------------------------------
// 見出し付きの区切り線
// -------------------------------------------------------------------------------
void EditorUI::SeparatorText(Context& _ctx, Font& _font, std::string_view _label)
{
	WindowFrame* pFrame = GetDrawableFrame(_ctx);
	if (pFrame == nullptr) { return; }

	const Style& style = _ctx.GetStyle();

	const std::wstring	wide		= StringUtil::Utf8ToWide(_label);
	const float			textWidth	= TextLayout::MeasureWidth(_font, wide);

	const DirectX::XMFLOAT2 size
	{
		GetContentWidth(pFrame, style),
		_font.GetLineHeight()
	};

	const Rect2D bounds = PlaceWidget(pFrame, size, style.ItemSpacing);

	// ラベルを左に置き、その右側だけに線を引く
	pFrame->Draw.PushClipRect(bounds);
	pFrame->Draw.AddText(bounds.Min, style.ColorTextMuted, wide, _font);
	pFrame->Draw.PopClipRect();

	const float lineY = bounds.Min.y + bounds.Height() * 0.5f;
	const float lineLeft = bounds.Min.x + textWidth + style.ItemInnerSpacing * 2.0f;

	if (lineLeft < bounds.Max.x)
	{
		pFrame->Draw.AddRectFilled(
			{ { lineLeft, lineY }, { bounds.Max.x, lineY + 1.0f } },
			style.ColorBorderLight);
	}
}

// -------------------------------------------------------------------------------
// 空白の場所取り
// -------------------------------------------------------------------------------
EditorUI::Rect2D EditorUI::Dummy(Context& _ctx, const DirectX::XMFLOAT2& _size)
{
	WindowFrame* pFrame = GetDrawableFrame(_ctx);
	if (pFrame == nullptr) { return {}; }

	return PlaceWidget(pFrame, _size, _ctx.GetStyle().ItemSpacing);
}

// -------------------------------------------------------------------------------
// テクスチャの表示
// -------------------------------------------------------------------------------
EditorUI::Rect2D EditorUI::Image(
	Context&					_ctx,
	TextureId					_texture,
	const DirectX::XMFLOAT2&	_size,
	const Rect2D&				_uv)
{
	WindowFrame* pFrame = GetDrawableFrame(_ctx);
	if (pFrame == nullptr) { return {}; }

	const Rect2D bounds = PlaceWidget(pFrame, _size, _ctx.GetStyle().ItemSpacing);

	pFrame->Draw.AddImage(bounds, _texture, _uv);

	return bounds;
}

// -------------------------------------------------------------------------------
// ポップアップ / コンテキストメニュー
// -------------------------------------------------------------------------------

// -------------------------------------------------------------------------------
// ウィンドウの空白部分での右クリックでメニューを開く
//
// 行の上での右クリックはBeginPopupContextItemが受け持つため、
// ここでは「どのウィジェットもホバーしていない」ことを条件にする
// -------------------------------------------------------------------------------
bool EditorUI::BeginPopupContextWindow(Context& _ctx, std::string_view _idLabel)
{
	const bool rightClickedOnEmptySpace =
		_ctx.IsMouseClicked(MouseButton::Mouse_Right) &&
		_ctx.IsCurrentWindowHovered() &&
		_ctx.GetHoveredId() == 0;

	if (rightClickedOnEmptySpace)
	{
		_ctx.OpenPopup(_idLabel);
	}

	return _ctx.BeginPopup(_idLabel);
}

// -------------------------------------------------------------------------------
// 直前のウィジェットに対するコンテキストメニュー
//
// 「右クリックされたか」は呼び出し側がItemInteractionから渡す
// ウィジェットごとに判定の仕方が違うため、条件はここで作らない
// -------------------------------------------------------------------------------
bool EditorUI::BeginPopupContextItem(Context& _ctx, std::string_view _idLabel, bool _rightClicked)
{
	if (_rightClicked)
	{
		_ctx.OpenPopup(_idLabel);
	}

	return _ctx.BeginPopup(_idLabel);
}

void EditorUI::OpenPopup(Context& _ctx, std::string_view _idLabel)
{
	_ctx.OpenPopup(_idLabel);
}

bool EditorUI::BeginPopup(Context& _ctx, std::string_view _idLabel)
{
	return _ctx.BeginPopup(_idLabel);
}

void EditorUI::EndPopup(Context& _ctx)
{
	_ctx.EndPopup();
}

// -------------------------------------------------------------------------------
// メニュー項目
// -------------------------------------------------------------------------------
bool EditorUI::MenuItem(
	Context&			_ctx,
	Font&				_font,
	std::string_view	_label,
	bool				_enabled)
{
	WindowFrame* pFrame = GetDrawableFrame(_ctx);
	if (pFrame == nullptr)
	{
		return false;
	}

	const Style&	style	= _ctx.GetStyle();
	const Id		id		= _ctx.GetIdStack().GetId(_label);

	const std::wstring	wide		= StringUtil::Utf8ToWide(_label);
	const float			textWidth	= TextLayout::MeasureWidth(_font, wide);

	// 項目の幅は「いちばん長い項目に合わせてメニュー全体が広がる」ようにしたいので、
	// 文字幅から必要な幅を申告する。実際のメニュー幅はContextがこれを集計して決める
	const DirectX::XMFLOAT2 size
	{
		(std::max)(textWidth + style.MenuPaddingX * 2.0f, GetContentWidth(pFrame, style)),
		style.MenuItemHeight
	};

	const Rect2D bounds = PlaceWidget(pFrame, size, 0.0f);

	if (!_enabled)
	{
		// 選べない項目は灰色で描くだけにして、当たり判定も行わない
		DrawRowLabel(*pFrame, _font, bounds, style.MenuPaddingX, wide, style.ColorTextDisabled);
		return false;
	}

	const InteractionState interaction = UpdateInteraction(_ctx, id, bounds);

	if (interaction.Hovered)
	{
		pFrame->Draw.AddRectFilled(bounds, style.ColorItemHovered);
	}

	DrawRowLabel(*pFrame, _font, bounds, style.MenuPaddingX, wide, style.ColorText);

	// 選ばれたらメニューを閉じる。呼び出し側がCloseを書き忘れても閉じ残らない
	if (interaction.Clicked)
	{
		_ctx.CloseCurrentPopup();
		return true;
	}

	return false;
}

void EditorUI::MenuSeparator(Context& _ctx)
{
	WindowFrame* pFrame = GetDrawableFrame(_ctx);
	if (pFrame == nullptr) { return; }

	const Style& style = _ctx.GetStyle();

	const Rect2D bounds = PlaceWidget(
		pFrame, { GetContentWidth(pFrame, style), 1.0f }, style.ItemSpacing);

	pFrame->Draw.AddRectFilled(bounds, style.ColorBorderLight);
}
