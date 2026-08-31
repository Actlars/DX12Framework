// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Widgets.h"
#include <Engine/EditorUI/Widgets/Layout/Layout.h>
#include <Engine/EditorUI/Widgets/WidgetInteraction.h>
#include <Engine/EditorUI/Widgets/WidgetPrimitives.h>
#include <Engine/EditorUI/Text/Font/Font.h>
#include <Engine/Utility/StringUtil/StringUtil.h>
#include <cwchar>	// swprintf_s / wcstod

// -------------------------------------------------------------------------------
// ValueWidgets
//
// 概要 :
//	変数を直接編集するウィジェット群(InputText / Drag* / Property)の実装
//
//	設計の中心にあるのは、次の2つの共通部品
//		TextFieldBehavior	キーボードによる文字列編集。キャレット・選択・確定を担う
//		DragScalarBehavior	マウスドラッグによる数値編集。クリックで上のものへ切り替わる
//
//	型ごとのDragXxxやPropertyは、この2つに引数を渡すだけの薄い層になっている
//	新しい型を足したくなった場合も、原則としてこの層に1つ追加するだけで済む
// -------------------------------------------------------------------------------
namespace
{
	using namespace EditorUI;

	// -------------------------------------------------------------------------------
	// 数値の書式化と解釈
	// -------------------------------------------------------------------------------

	// 表示できる小数桁数の上限。バッファ長を超える書式を防ぐ
	constexpr int kMaxPrecision = 9;

	// -------------------------------------------------------------------------------
	// 数値を表示用の文字列にする
	//
	// 型ごとに書式指定子が変わる部分だけを if constexpr で分岐し、
	// 呼び出し側からは型を意識せず使えるようにしている
	// -------------------------------------------------------------------------------
	template<typename T>
	std::wstring FormatNumber(T _value, int _precision)
	{
		wchar_t buffer[64] = {};

		if constexpr (std::is_same_v<T, int32_t>)
		{
			swprintf_s(buffer, L"%d", _value);
		}
		else if constexpr (std::is_same_v<T, uint32_t>)
		{
			swprintf_s(buffer, L"%u", _value);
		}
		else
		{
			const int precision = std::clamp(_precision, 0, kMaxPrecision);
			swprintf_s(buffer, L"%.*f", precision, static_cast<double>(_value));
		}

		return buffer;
	}

	// -------------------------------------------------------------------------------
	// doubleを、あふれさせずにT型へ変換する
	//
	// 入力欄には桁数の制限がないため、型の表現範囲を大きく超える値を打ち込める
	// そのままキャストすると未定義動作になるので、必ずここを通す
	// -------------------------------------------------------------------------------
	template<typename T>
	T SaturateCast(double _value)
	{
		constexpr T lowest	= (std::numeric_limits<T>::lowest)();
		constexpr T highest	= (std::numeric_limits<T>::max)();

		if (!std::isfinite(_value))
		{
			return static_cast<T>(0);
		}

		if (_value <= static_cast<double>(lowest))	{ return lowest; }
		if (_value >= static_cast<double>(highest))	{ return highest; }

		if constexpr (std::is_integral_v<T>)
		{
			// 整数欄に "1.6" と打たれた場合、切り捨てより四捨五入のほうが直感的
			return static_cast<T>(std::llround(_value));
		}
		else
		{
			return static_cast<T>(_value);
		}
	}

	// -------------------------------------------------------------------------------
	// 文字列を数値として解釈する
	//
	// 整数型でも一度doubleとして読むのは、"1e3" や "2.5" のような入力を
	// エラーにせず受け入れるため
	//
	// @return	1文字も数値として読めなければfalse
	// -------------------------------------------------------------------------------
	template<typename T>
	bool ParseNumber(std::wstring_view _text, T& _outValue)
	{
		// wcstodはヌル終端を要求するため、ここで一度実体化する
		const std::wstring text(_text);

		wchar_t* pEnd = nullptr;
		const double parsed = std::wcstod(text.c_str(), &pEnd);

		if (pEnd == text.c_str())
		{
			return false;	// 数字が1文字も含まれていない
		}

		_outValue = SaturateCast<T>(parsed);
		return true;
	}

	// オプションのMin/Maxと刻み幅を値に適用する
	template<typename T>
	T ApplyNumericConstraints(T _value, const NumericEditorOptions<T>& _options)
	{
		T value = _value;

		// Stepが指定されていれば、その倍数へ丸める
		if constexpr (std::is_floating_point_v<T>)
		{
			if (_options.Step > static_cast<T>(0))
			{
				value = static_cast<T>(std::round(value / _options.Step)) * _options.Step;
			}
		}

		if (_options.Clamp)
		{
			// Min > Max のような指定でも壊れないよう、順序をそろえてから挟む
			const T low  = (std::min)(_options.Min, _options.Max);
			const T high = (std::max)(_options.Min, _options.Max);
			value = std::clamp(value, low, high);
		}

		return value;
	}

	// -------------------------------------------------------------------------------
	// テキスト編集
	// -------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------
	// TextFieldConfig struct
	//
	// 概要 :
	//	TextFieldBehaviorの挙動の指定
	//	InputTextとDrag系で共通の入力欄を使いつつ、違いをここで吸収する
	// -------------------------------------------------------------------------------
	struct TextFieldConfig
	{
		std::size_t MaxCharacters	= 4096;
		bool		ReadOnly		= false;
		bool		CommitOnFocusLoss = true;	// 他所をクリックしたときに確定するか
		bool		SelectAllOnFocus  = false;	// 編集開始時に全選択するか

		// クリックだけで編集を始めてよいか
		// Drag系は「クリック＝ドラッグ開始」と紛れるためfalseにし、
		// ドラッグされずに離されたと確定してから外部より編集を開始させる
		bool		ActivateOnClick	= true;

		TextHorizontalAlignment Alignment = TextHorizontalAlignment::Left;
	};

	// -------------------------------------------------------------------------------
	// TextFieldResult struct
	// -------------------------------------------------------------------------------
	struct TextFieldResult
	{
		bool			Editing		= false;	// このフレーム、キーボード編集中か
		bool			Committed	= false;	// このフレームで確定したか
		std::wstring	Text;					// 確定した文字列(Committed時のみ有効)
	};

	// 選択範囲があればそれを削除し、キャレットを詰める
	void EraseSelection(TextEditState& _edit)
	{
		if (!_edit.HasSelection())
		{
			return;
		}

		const int begin = _edit.SelectionBegin();
		const int end	= _edit.SelectionEnd();

		_edit.Text.erase(static_cast<std::size_t>(begin), static_cast<std::size_t>(end - begin));
		_edit.CursorIndex = begin;
		_edit.ClearSelection();
	}

	// キャレットを移動する。Shift押下中は選択の始点を動かさず、範囲を伸ばす
	void MoveCursor(TextEditState& _edit, int _newIndex, bool _keepSelection)
	{
		_edit.CursorIndex = std::clamp(_newIndex, 0, static_cast<int>(_edit.Text.size()));

		if (!_keepSelection)
		{
			_edit.ClearSelection();
		}
	}

	// -------------------------------------------------------------------------------
	// キーボード入力を編集状態へ反映する
	//
	// @return	Enterで確定されたらtrue
	// -------------------------------------------------------------------------------
	bool ApplyKeyboardInput(Context& _ctx, TextEditState& _edit, const TextFieldConfig& _config)
	{
		const bool shift	= _ctx.IsKeyDown(Key::Shift);
		const bool ctrl		= _ctx.IsKeyDown(Key::Ctrl);

		// Ctrl + A で全選択
		if (ctrl && _ctx.IsKeyPressed(Key::A))
		{
			_edit.SelectionAnchor	= 0;
			_edit.CursorIndex		= static_cast<int>(_edit.Text.size());
			return false;
		}

		// キャレット移動
		if (_ctx.IsKeyPressed(Key::Left))	{ MoveCursor(_edit, _edit.CursorIndex - 1, shift); }
		if (_ctx.IsKeyPressed(Key::Right))	{ MoveCursor(_edit, _edit.CursorIndex + 1, shift); }
		if (_ctx.IsKeyPressed(Key::Home))	{ MoveCursor(_edit, 0, shift); }
		if (_ctx.IsKeyPressed(Key::End))	{ MoveCursor(_edit, static_cast<int>(_edit.Text.size()), shift); }

		if (_config.ReadOnly)
		{
			// 読み取り専用でも選択とコピー操作のためにキャレットは動かすが、書き換えはしない
			return _ctx.IsKeyPressed(Key::Enter);
		}

		// BackSpace : 選択があればまとめて削除、なければ1文字戻って削除
		if (_ctx.IsKeyPressed(Key::Backspace))
		{
			if (_edit.HasSelection())
			{
				EraseSelection(_edit);
			}
			else if (_edit.CursorIndex > 0)
			{
				_edit.Text.erase(static_cast<std::size_t>(_edit.CursorIndex - 1), 1);
				--_edit.CursorIndex;
				_edit.ClearSelection();
			}
		}

		// Delete : キャレットの右側を削除
		if (_ctx.IsKeyPressed(Key::Delete))
		{
			if (_edit.HasSelection())
			{
				EraseSelection(_edit);
			}
			else if (_edit.CursorIndex < static_cast<int>(_edit.Text.size()))
			{
				_edit.Text.erase(static_cast<std::size_t>(_edit.CursorIndex), 1);
			}
		}

		// 文字の挿入
		for (const wchar_t ch : _ctx.GetInputCharacters())
		{
			// 制御文字(Enter / Tab / BackSpaceなど)はキー側で処理済みなので入れない
			if (ch < 0x20 || ch == 0x7F)
			{
				continue;
			}

			if (_edit.Text.size() >= _config.MaxCharacters)
			{
				break;
			}

			EraseSelection(_edit);	// 選択中に打てば置き換えになる
			_edit.Text.insert(static_cast<std::size_t>(_edit.CursorIndex), 1, ch);
			++_edit.CursorIndex;
			_edit.ClearSelection();
		}

		return _ctx.IsKeyPressed(Key::Enter);
	}

	// -------------------------------------------------------------------------------
	// 編集中のテキストが入力欄からはみ出さないよう、横スクロール量を調整する
	//
	// キャレットは常に見えている必要があるため、キャレットを基準に寄せる
	// -------------------------------------------------------------------------------
	void UpdateTextScroll(Font& _font, TextEditState& _edit, float _visibleWidth)
	{
		const float cursorX = TextLayout::MeasureWidthUpTo(
			_font, _edit.Text, static_cast<std::size_t>(_edit.CursorIndex));

		// キャレットが左端より外にある
		if (cursorX < _edit.ScrollX)
		{
			_edit.ScrollX = cursorX;
		}

		// キャレットが右端より外にある
		if (cursorX > _edit.ScrollX + _visibleWidth)
		{
			_edit.ScrollX = cursorX - _visibleWidth;
		}

		// 文字列が縮んで余白ができた場合は、左へ詰め直す
		const float totalWidth = TextLayout::MeasureWidth(_font, _edit.Text);
		_edit.ScrollX = std::clamp(_edit.ScrollX, 0.0f, (std::max)(0.0f, totalWidth - _visibleWidth));
	}

	// -------------------------------------------------------------------------------
	// 指定した矩形に文字列入力欄を描き、キーボードによる編集を処理する
	//
	//	編集中の文字列はContextのTextEditStateに置かれ、確定するまで
	//	呼び出し元の変数には書き戻されない
	//	これにより、入力途中の中途半端な文字列("-" だけ、"1.e" など)が
	//	アプリ側の変数に流れ込むのを防いでいる
	//
	// @param[in]	_ctx			描画対象のContext
	// @param[in]	_id				この入力欄のId
	// @param[in]	_font			描画に使うフォント
	// @param[in]	_bounds			描画する矩形
	// @param[in]	_displayText	編集中でないときに表示する文字列
	// @param[in]	_config			挙動の指定
	// @param[in]	_forceActivate	外部から編集を開始させたい場合にtrue
	// @param[in]	_background		背景色
	// @return	編集状態と、確定した文字列
	// -------------------------------------------------------------------------------
	TextFieldResult TextFieldBehavior(
		Context&				_ctx,
		Id						_id,
		Font&					_font,
		const Rect2D&			_bounds,
		std::wstring_view		_displayText,
		const TextFieldConfig&	_config,
		bool					_forceActivate,
		Color32					_background)
	{
		TextFieldResult result;

		WindowFrame* pFrame = _ctx.GetCurrentWindow();
		if (pFrame == nullptr)
		{
			return result;
		}

		const Style&	style	= _ctx.GetStyle();
		TextEditState&	edit	= _ctx.GetTextEditState();

		const bool	hovered		= _bounds.Contains(_ctx.GetMousePos()) && _ctx.IsCurrentWindowHovered();
		bool		editing		= (edit.Widget == _id);

		// -------------------------------------------------------------------------------
		// 編集の開始
		// -------------------------------------------------------------------------------
		const bool clickToEdit = _config.ActivateOnClick && hovered &&
			_ctx.IsMouseClicked(MouseButton::Mouse_Left) && !_ctx.IsAnyItemActive();

		if (!editing && !_config.ReadOnly && (_forceActivate || clickToEdit))
		{
			edit.Reset();
			edit.Widget	= _id;
			edit.Text	= std::wstring(_displayText);

			// ドラッグから切り替わってきた場合は、そのまま打ち直せるよう全選択する
			if (_config.SelectAllOnFocus || _forceActivate)
			{
				edit.SelectionAnchor	= 0;
				edit.CursorIndex		= static_cast<int>(edit.Text.size());
			}
			else
			{
				// クリック位置の文字へキャレットを置く
				const float offsetX = _ctx.GetMousePos().x - (_bounds.Min.x + style.FramePaddingX);
				edit.CursorIndex = static_cast<int>(
					TextLayout::FindCharacterIndexAtX(_font, edit.Text, offsetX));
				edit.ClearSelection();
			}

			_ctx.SetActive(_id);
			editing = true;
		}

		// -------------------------------------------------------------------------------
		// 編集中の入力処理
		// -------------------------------------------------------------------------------
		const float textAreaWidth = (std::max)(0.0f, _bounds.Width() - style.FramePaddingX * 2.0f);

		if (editing)
		{
			edit.Alive = true;	// Contextに「まだ生きている」と申告する

			// 所有権を取り直さず、持ち続けている場合だけ延長する
			// ここでSetActiveIdを呼ぶと、他のウィジェットへ移った所有権を
			// 奪い返してしまい、編集が終わらなくなる
			_ctx.KeepActiveIdAlive(_id);

			const bool enterPressed	 = ApplyKeyboardInput(_ctx, edit, _config);
			const bool escapePressed = _ctx.IsKeyPressed(Key::Escape);

			// 入力欄の中をドラッグして範囲選択する
			if (_ctx.IsMouseDown(MouseButton::Mouse_Left) && !_ctx.IsMouseClicked(MouseButton::Mouse_Left) && hovered)
			{
				const float offsetX = _ctx.GetMousePos().x - (_bounds.Min.x + style.FramePaddingX) + edit.ScrollX;
				edit.CursorIndex = static_cast<int>(
					TextLayout::FindCharacterIndexAtX(_font, edit.Text, offsetX));
			}

			UpdateTextScroll(_font, edit, textAreaWidth);

			// -------------------------------------------------------------------------------
			// 編集の終了
			//
			//	他所のクリックは、そのウィジェットがActiveIdを奪ったかどうかで判定できる
			//	何もない場所をクリックした場合は誰も奪わないため、位置でも判定する
			// -------------------------------------------------------------------------------
			const bool clickedOutside	= _ctx.IsMouseClicked(MouseButton::Mouse_Left) && !hovered;
			const bool lostFocus		= clickedOutside || !_ctx.IsActiveId(_id);

			if (escapePressed)
			{
				// 破棄。呼び出し元の値は書き換えない
				_ctx.ClearActiveId(_id);
				edit.Reset();
				editing = false;
			}
			else if (enterPressed || (lostFocus && _config.CommitOnFocusLoss))
			{
				result.Committed	= true;
				result.Text			= edit.Text;

				_ctx.ClearActiveId(_id);
				edit.Reset();
				editing = false;
			}
			else if (lostFocus)
			{
				_ctx.ClearActiveId(_id);
				edit.Reset();
				editing = false;
			}
		}

		result.Editing = editing;

		// -------------------------------------------------------------------------------
		// 描画
		// -------------------------------------------------------------------------------
		DrawFrame(*pFrame, _bounds, _background, style);

		// 表示する文字列と、その描画開始位置を決める
		const std::wstring_view text = editing ? std::wstring_view(edit.Text) : _displayText;
		const float textWidth	= TextLayout::MeasureWidth(_font, text);
		const float scrollX		= editing ? edit.ScrollX : 0.0f;

		// 編集中は行揃えを無視して左詰めにする
		// キャレットの位置と文字の位置がずれないようにするため
		const float alignOffsetX = editing
			? 0.0f
			: TextLayout::ResolveAlignmentOffsetX(_config.Alignment, textAreaWidth, textWidth);

		const float textX = _bounds.Min.x + style.FramePaddingX + alignOffsetX - scrollX;
		const float textY = _bounds.Min.y + (_bounds.Height() - _font.GetLineHeight()) * 0.5f;

		// 枠からはみ出す文字を隠す
		pFrame->Draw.PushClipRect(_bounds);

		// 選択範囲は文字の下に敷く
		if (editing && edit.HasSelection())
		{
			const float selectBeginX = TextLayout::MeasureWidthUpTo(
				_font, text, static_cast<std::size_t>(edit.SelectionBegin()));
			const float selectEndX = TextLayout::MeasureWidthUpTo(
				_font, text, static_cast<std::size_t>(edit.SelectionEnd()));

			const Rect2D selection =
			{
				{ textX + selectBeginX, textY },
				{ textX + selectEndX,   textY + _font.GetLineHeight() }
			};
			pFrame->Draw.AddRectFilled(selection, style.ColorTextSelectionBg);
		}

		const Color32 textColor = _config.ReadOnly ? style.ColorTextDisabled : style.ColorText;
		pFrame->Draw.AddText({ textX, textY }, textColor, text, _font);

		// キャレット
		if (editing && !_config.ReadOnly)
		{
			const float cursorX = TextLayout::MeasureWidthUpTo(
				_font, text, static_cast<std::size_t>(edit.CursorIndex));

			const Rect2D cursor =
			{
				{ textX + cursorX,						textY },
				{ textX + cursorX + style.TextCursorWidth, textY + _font.GetLineHeight() }
			};
			pFrame->Draw.AddRectFilled(cursor, style.ColorTextCursor);
		}

		pFrame->Draw.PopClipRect();

		return result;
	}

	// -------------------------------------------------------------------------------
	// 数値編集
	// -------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------
	// ドラッグ量を値に反映する
	//
	//	整数型では、1ピクセル動かしただけでは1に満たないことがある
	//	端数をContextに持ち越すことで、ゆっくり動かしても値が変わらない、という
	//	操作感の悪さを避けている
	//
	// @return	値が変化したらtrue
	// -------------------------------------------------------------------------------
	template<typename T>
	bool ApplyDragDelta(Context& _ctx, T* _value, float _mouseDeltaX, const NumericEditorOptions<T>& _options)
	{
		if (_mouseDeltaX == 0.0f)
		{
			return false;
		}

		double& accumulator = _ctx.GetActiveDragAccumulator();
		accumulator += static_cast<double>(_mouseDeltaX) * static_cast<double>(_options.DragSpeed);

		double applied = 0.0;

		if constexpr (std::is_floating_point_v<T>)
		{
			// 浮動小数点はそのまま足せるので、端数を残す必要がない
			applied		= accumulator;
			accumulator	= 0.0;
		}
		else
		{
			// 整数は1未満を反映できないため、整数部だけ使って残りを持ち越す
			applied		 = std::trunc(accumulator);
			accumulator -= applied;
		}

		if (applied == 0.0)
		{
			return false;
		}

		const T before	= *_value;
		const T after	= ApplyNumericConstraints(
			SaturateCast<T>(static_cast<double>(before) + applied), _options);

		*_value = after;
		return after != before;
	}

	// -------------------------------------------------------------------------------
	// 指定した矩形に数値編集欄を描く
	//
	//	操作は2つのモードを行き来する
	//		ドラッグ	: 左右の動きで値を増減する
	//		キーボード	: ドラッグせずにクリックすると切り替わり、直接打ち込める
	//
	// @param[in]		_ctx		描画対象のContext
	// @param[in]		_id			この編集欄のId
	// @param[in]		_font		描画に使うフォント
	// @param[in]		_bounds		描画する矩形
	// @param[in,out]	_value		編集対象の値
	// @param[in]		_options	範囲・刻み幅などの指定
	// @return	true : このフレームで値が変化した / false : それ以外
	// -------------------------------------------------------------------------------
	template<typename T>
	bool DragScalarBehavior(
		Context&							_ctx,
		Id									_id,
		Font&								_font,
		const Rect2D&						_bounds,
		T*									_value,
		const NumericEditorOptions<T>&		_options)
	{
		if (_value == nullptr)
		{
			return false;
		}

		const Style&	style	= _ctx.GetStyle();
		TextEditState&	edit	= _ctx.GetTextEditState();

		const bool	isEditingText	= (edit.Widget == _id);
		bool		valueChanged	= false;
		bool		beginTextEdit	= false;

		Color32 background = style.ColorFrameBg;

		// -------------------------------------------------------------------------------
		// キーボード編集中でなければ、ドラッグとして扱う
		// -------------------------------------------------------------------------------
		if (!isEditingText)
		{
			if (_options.ReadOnly)
			{
				background = style.ColorFrameBgReadOnly;
			}
			else
			{
				const InteractionState interaction = UpdateInteraction(_ctx, _id, _bounds);

				if (interaction.Held)
				{
					valueChanged = ApplyDragDelta(_ctx, _value, _ctx.GetMouseDelta().x, _options);
				}

				// ほとんど動かさずに離した＝クリック。キーボード入力へ切り替える
				if (interaction.Deactivated && interaction.Clicked)
				{
					const DirectX::XMFLOAT2 clickPos = _ctx.GetActiveIdClickPos();
					const float dx = _ctx.GetMousePos().x - clickPos.x;
					const float dy = _ctx.GetMousePos().y - clickPos.y;

					const float thresholdSquared = style.DragActivationThreshold * style.DragActivationThreshold;
					if ((dx * dx + dy * dy) < thresholdSquared)
					{
						beginTextEdit = true;
					}
				}

				background = interaction.Held		? style.ColorFrameBgActive
						   : interaction.Hovered	? style.ColorFrameBgHovered
						   : style.ColorFrameBg;
			}
		}
		else
		{
			background = style.ColorFrameBgActive;
		}

		// -------------------------------------------------------------------------------
		// 表示と、キーボード編集の処理
		// -------------------------------------------------------------------------------
		TextFieldConfig config;
		config.MaxCharacters	= 32;
		config.ReadOnly			= _options.ReadOnly;
		config.ActivateOnClick	= false;	// 切り替えはドラッグ判定の結果から行う
		config.SelectAllOnFocus	= true;
		config.Alignment		= TextHorizontalAlignment::Center;

		const std::wstring display = FormatNumber(*_value, _options.Precision);

		const TextFieldResult field = TextFieldBehavior(
			_ctx, _id, _font, _bounds, display, config, beginTextEdit, background);

		if (field.Committed)
		{
			T parsed{};
			if (ParseNumber(field.Text, parsed))
			{
				const T constrained = ApplyNumericConstraints(parsed, _options);
				if (constrained != *_value)
				{
					*_value			= constrained;
					valueChanged	= true;
				}
			}
		}

		return valueChanged;
	}

	// -------------------------------------------------------------------------------
	// プロパティ行
	// -------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------
	// PropertyRow struct
	//
	// 概要 :
	//	「ラベル + 編集欄」の1行を確保した結果
	// -------------------------------------------------------------------------------
	struct PropertyRow
	{
		bool	Valid		= false;	// 行を確保できたか
		Id		BaseId		= 0;		// 編集欄のIdを派生させるもとになるId
		Rect2D	EditorRect{};			// 編集欄に使える矩形
	};

	// 行の高さを決める。指定がなければフォントの行の高さに余白を足す
	float ResolveRowHeight(const PropertyLayout& _layout, const Font& _font, const Style& _style)
	{
		if (_layout.RowHeight > 0.0f)
		{
			return _layout.RowHeight;
		}

		return _font.GetLineHeight() + _style.FramePaddingY * 2.0f;
	}

	// -------------------------------------------------------------------------------
	// プロパティ行を1行ぶん確保し、ラベルを描く
	//
	//	ラベルの幅をそろえることで、複数のプロパティを並べたときに
	//	編集欄の左端が縦にそろい、インスペクタとして読みやすくなる
	// -------------------------------------------------------------------------------
	PropertyRow BeginPropertyRow(
		Context&				_ctx,
		Font&					_font,
		std::string_view		_label,
		const PropertyLayout&	_layout)
	{
		PropertyRow row;

		WindowFrame* pFrame = _ctx.GetCurrentWindow();
		if (pFrame == nullptr || pFrame->SkipContents)
		{
			return row;
		}

		const Style& style = _ctx.GetStyle();

		const float rowHeight		= ResolveRowHeight(_layout, _font, style);
		const float availableWidth	= GetContentWidth(pFrame, style);
		const Rect2D bounds			= PlaceWidget(pFrame, { availableWidth, rowHeight }, style.ItemSpacing);

		// ラベルが行幅を食いつぶして編集欄が消えないよう、上限を設ける
		const float labelWidth = (std::min)(_layout.LabelWidth, availableWidth * 0.6f);

		// -------------------------------------------------------------------------------
		// ラベル
		// -------------------------------------------------------------------------------
		const std::wstring wideLabel = StringUtil::Utf8ToWide(_label);

		const Rect2D labelRect
		{
			bounds.Min,
			{ bounds.Min.x + labelWidth, bounds.Max.y }
		};

		TextOptions labelOptions;
		labelOptions.Wrap		= false;
		labelOptions.MaxLines	= 1;
		labelOptions.Ellipsis	= true;	// 長いラベルは "..." で省略する

		std::vector<TextLine> lines;
		TextLayout::Build(_font, wideLabel, labelOptions,
			(std::max)(0.0f, labelWidth - style.ItemInnerSpacing), lines);

		const float labelY = labelRect.Min.y + (rowHeight - _font.GetLineHeight()) * 0.5f;

		pFrame->Draw.PushClipRect(labelRect);
		if (!lines.empty())
		{
			const TextLine& line = lines.front();
			const std::wstring_view labelText = std::wstring_view(wideLabel).substr(line.Begin, line.End - line.Begin);

			pFrame->Draw.AddText({ labelRect.Min.x, labelY }, style.ColorText, labelText, _font);

			if (line.Ellipsis)
			{
				const float width = TextLayout::MeasureWidth(_font, labelText);
				pFrame->Draw.AddText(
					{ labelRect.Min.x + width, labelY }, style.ColorText, TextLayout::kEllipsis, _font);
			}
		}
		pFrame->Draw.PopClipRect();

		row.Valid		= true;
		row.BaseId		= _ctx.GetIdStack().GetId(_label);
		row.EditorRect	=
		{
			{ bounds.Min.x + labelWidth + style.ItemInnerSpacing, bounds.Min.y },
			bounds.Max
		};

		return row;
	}

	// 成分ごとのアクセント色。XYZWの順に対応する
	Color32 GetAxisColor(const Style& _style, int _componentIndex)
	{
		switch (_componentIndex)
		{
		case 0:  return _style.ColorAxisX;
		case 1:  return _style.ColorAxisY;
		case 2:  return _style.ColorAxisZ;
		default: return _style.ColorAxisW;
		}
	}

	// -------------------------------------------------------------------------------
	// 複数成分(XMFLOAT2/3/4)を横に並べて編集する
	//
	//	各成分は左端の色帯で見分けられるようにし、Idは成分番号を混ぜて衝突を防ぐ
	//
	// @param[in]		_ctx			描画対象のContext
	// @param[in]		_font			描画に使うフォント
	// @param[in]		_label			ラベル
	// @param[in,out]	_components		成分の先頭アドレス
	// @param[in]		_componentCount	成分数
	// @param[in]		_options		数値編集の指定
	// @param[in]		_layout			行レイアウト
	// @return	true : いずれかの成分が変化した / false : それ以外
	// -------------------------------------------------------------------------------
	bool PropertyVector(
		Context&							_ctx,
		Font&								_font,
		std::string_view					_label,
		float*								_components,
		int									_componentCount,
		const NumericEditorOptions<float>&	_options,
		const PropertyLayout&				_layout)
	{
		if (_components == nullptr || _componentCount <= 0)
		{
			return false;
		}

		const PropertyRow row = BeginPropertyRow(_ctx, _font, _label, _layout);
		if (!row.Valid)
		{
			return false;
		}

		WindowFrame*	pFrame	= _ctx.GetCurrentWindow();
		const Style&	style	= _ctx.GetStyle();

		// 成分どうしの間隔を除いた残りを均等に割り当てる
		const float totalSpacing	= _layout.ComponentSpacing * static_cast<float>(_componentCount - 1);
		const float componentWidth	= (row.EditorRect.Width() - totalSpacing) / static_cast<float>(_componentCount);

		// 幅が取れないほど狭い場合は、描いても操作できないので何もしない
		constexpr float kMinComponentWidth = 8.0f;
		if (componentWidth < kMinComponentWidth)
		{
			return false;
		}

		constexpr float kAccentWidth = 3.0f;

		bool valueChanged = false;

		for (int i = 0; i < _componentCount; ++i)
		{
			const float left = row.EditorRect.Min.x +
				static_cast<float>(i) * (componentWidth + _layout.ComponentSpacing);

			const Rect2D componentRect
			{
				{ left, row.EditorRect.Min.y },
				{ left + componentWidth, row.EditorRect.Max.y }
			};

			// 同じラベルでも成分ごとに別のウィジェットとして扱えるよう、番号を混ぜる
			const Id componentId = HashInt(i, row.BaseId);

			valueChanged |= DragScalarBehavior(_ctx, componentId, _font, componentRect, &_components[i], _options);

			// 左端にX/Y/Z/Wを表す色帯を重ねる。編集欄の上に描くことで枠線に隠れない
			const Rect2D accent
			{
				componentRect.Min,
				{ componentRect.Min.x + kAccentWidth, componentRect.Max.y }
			};
			pFrame->Draw.AddRectFilled(accent, GetAxisColor(style, i));
		}

		return valueChanged;
	}

	// 単一の数値プロパティ。型ごとのProperty()はすべてここへ合流する
	template<typename T>
	bool PropertyScalar(
		Context&						_ctx,
		Font&							_font,
		std::string_view				_label,
		T*								_value,
		const NumericEditorOptions<T>&	_options,
		const PropertyLayout&			_layout)
	{
		if (_value == nullptr)
		{
			return false;
		}

		const PropertyRow row = BeginPropertyRow(_ctx, _font, _label, _layout);
		if (!row.Valid)
		{
			return false;
		}

		return DragScalarBehavior(_ctx, row.BaseId, _font, row.EditorRect, _value, _options);
	}

	// -------------------------------------------------------------------------------
	// レイアウトに1行を消費して数値編集欄を置く。DragXxxの共通実装
	// -------------------------------------------------------------------------------
	template<typename T>
	bool DragScalarWidget(
		Context&						_ctx,
		std::string_view				_idLabel,
		Font&							_font,
		T*								_value,
		const NumericEditorOptions<T>&	_options)
	{
		WindowFrame* pFrame = _ctx.GetCurrentWindow();
		if (pFrame == nullptr || pFrame->SkipContents || _value == nullptr)
		{
			return false;
		}

		const Style& style = _ctx.GetStyle();

		const float width	= ConsumeNextItemWidth(pFrame, GetContentWidth(pFrame, style));
		const float height	= _font.GetLineHeight() + style.FramePaddingY * 2.0f;

		const Rect2D bounds	= PlaceWidget(pFrame, { width, height }, style.ItemSpacing);
		const Id	 id		= _ctx.GetIdStack().GetId(_idLabel);

		return DragScalarBehavior(_ctx, id, _font, bounds, _value, _options);
	}
}

// -------------------------------------------------------------------------------
// 文字列入力
// -------------------------------------------------------------------------------
bool EditorUI::InputText(
	Context&				_ctx,
	std::string_view		_idLabel,
	Font&					_font,
	std::string*			_value,
	const InputTextOptions& _options)
{
	WindowFrame* pFrame = _ctx.GetCurrentWindow();
	if (pFrame == nullptr || pFrame->SkipContents || _value == nullptr)
	{
		return false;
	}

	const Style& style = _ctx.GetStyle();

	// サイズの0以下の成分は自動決定する
	const float width = (_options.Size.x > 0.0f)
		? _options.Size.x
		: ConsumeNextItemWidth(pFrame, GetContentWidth(pFrame, style));

	const float height = (_options.Size.y > 0.0f)
		? _options.Size.y
		: _font.GetLineHeight() + style.FramePaddingY * 2.0f;

	const Rect2D bounds	= PlaceWidget(pFrame, { width, height }, style.ItemSpacing);
	const Id	 id		= _ctx.GetIdStack().GetId(_idLabel);

	TextFieldConfig config;
	config.MaxCharacters		= _options.MaxCharacters;
	config.ReadOnly				= _options.ReadOnly;
	config.CommitOnFocusLoss	= _options.CommitOnFocusLoss;
	config.SelectAllOnFocus		= _options.SelectAllOnFocus;
	config.ActivateOnClick		= true;

	const bool hovered	= bounds.Contains(_ctx.GetMousePos()) && _ctx.IsCurrentWindowHovered();
	const bool editing	= _ctx.GetTextEditState().Widget == id;

	const Color32 background = _options.ReadOnly	? style.ColorFrameBgReadOnly
							 : editing				? style.ColorFrameBgActive
							 : hovered				? style.ColorFrameBgHovered
							 : style.ColorFrameBg;

	const std::wstring display = StringUtil::Utf8ToWide(*_value);

	// ActivateNowが指定された場合、クリックを待たずにその場で編集を始める
	const TextFieldResult result = TextFieldBehavior(
		_ctx, id, _font, bounds, display, config, _options.ActivateNow, background);

	if (!result.Committed)
	{
		return false;
	}

	// 確定した内容が元と同じなら、変更されたとは報告しない
	std::string committed = StringUtil::WideToUtf8(result.Text);
	if (committed == *_value)
	{
		return false;
	}

	*_value = std::move(committed);
	return true;
}

// -------------------------------------------------------------------------------
// 数値のドラッグ編集
// -------------------------------------------------------------------------------
bool EditorUI::DragInt32(Context& _ctx, std::string_view _idLabel, Font& _font, int32_t* _value, const NumericEditorOptions<int32_t>& _options)
{
	return DragScalarWidget(_ctx, _idLabel, _font, _value, _options);
}

bool EditorUI::DragUInt32(Context& _ctx, std::string_view _idLabel, Font& _font, uint32_t* _value, const NumericEditorOptions<uint32_t>& _options)
{
	return DragScalarWidget(_ctx, _idLabel, _font, _value, _options);
}

bool EditorUI::DragFloat(Context& _ctx, std::string_view _idLabel, Font& _font, float* _value, const NumericEditorOptions<float>& _options)
{
	return DragScalarWidget(_ctx, _idLabel, _font, _value, _options);
}

bool EditorUI::DragDouble(Context& _ctx, std::string_view _idLabel, Font& _font, double* _value, const NumericEditorOptions<double>& _options)
{
	return DragScalarWidget(_ctx, _idLabel, _font, _value, _options);
}

// -------------------------------------------------------------------------------
// プロパティ行
// -------------------------------------------------------------------------------
bool EditorUI::Property(Context& _ctx, Font& _font, std::string_view _label, bool* _value, const PropertyLayout& _layout)
{
	if (_value == nullptr)
	{
		return false;
	}

	const PropertyRow row = BeginPropertyRow(_ctx, _font, _label, _layout);
	if (!row.Valid)
	{
		return false;
	}

	// チェックボックスは正方形にしたいので、行の高さに合わせた一辺で切り出す
	const float boxSize = (std::min)(row.EditorRect.Height(), row.EditorRect.Width());

	const Rect2D boxRect
	{
		row.EditorRect.Min,
		{ row.EditorRect.Min.x + boxSize, row.EditorRect.Min.y + boxSize }
	};

	return CheckboxBehavior(_ctx, row.BaseId, boxRect, _value);
}

bool EditorUI::Property(Context& _ctx, Font& _font, std::string_view _label, std::string* _value, const InputTextOptions& _options, const PropertyLayout& _layout)
{
	if (_value == nullptr)
	{
		return false;
	}

	const PropertyRow row = BeginPropertyRow(_ctx, _font, _label, _layout);
	if (!row.Valid)
	{
		return false;
	}

	const Style& style = _ctx.GetStyle();

	TextFieldConfig config;
	config.MaxCharacters		= _options.MaxCharacters;
	config.ReadOnly				= _options.ReadOnly;
	config.CommitOnFocusLoss	= _options.CommitOnFocusLoss;
	config.SelectAllOnFocus		= _options.SelectAllOnFocus;
	config.ActivateOnClick		= true;

	const bool hovered	= row.EditorRect.Contains(_ctx.GetMousePos()) && _ctx.IsCurrentWindowHovered();
	const bool editing	= _ctx.GetTextEditState().Widget == row.BaseId;

	const Color32 background = _options.ReadOnly	? style.ColorFrameBgReadOnly
							 : editing				? style.ColorFrameBgActive
							 : hovered				? style.ColorFrameBgHovered
							 : style.ColorFrameBg;

	const TextFieldResult result = TextFieldBehavior(
		_ctx, row.BaseId, _font, row.EditorRect, StringUtil::Utf8ToWide(*_value), config, false, background);

	if (!result.Committed)
	{
		return false;
	}

	std::string committed = StringUtil::WideToUtf8(result.Text);
	if (committed == *_value)
	{
		return false;
	}

	*_value = std::move(committed);
	return true;
}

bool EditorUI::Property(Context& _ctx, Font& _font, std::string_view _label, int32_t* _value, const NumericEditorOptions<int32_t>& _options, const PropertyLayout& _layout)
{
	return PropertyScalar(_ctx, _font, _label, _value, _options, _layout);
}

bool EditorUI::Property(Context& _ctx, Font& _font, std::string_view _label, uint32_t* _value, const NumericEditorOptions<uint32_t>& _options, const PropertyLayout& _layout)
{
	return PropertyScalar(_ctx, _font, _label, _value, _options, _layout);
}

bool EditorUI::Property(Context& _ctx, Font& _font, std::string_view _label, float* _value, const NumericEditorOptions<float>& _options, const PropertyLayout& _layout)
{
	return PropertyScalar(_ctx, _font, _label, _value, _options, _layout);
}

bool EditorUI::Property(Context& _ctx, Font& _font, std::string_view _label, double* _value, const NumericEditorOptions<double>& _options, const PropertyLayout& _layout)
{
	return PropertyScalar(_ctx, _font, _label, _value, _options, _layout);
}

bool EditorUI::Property(Context& _ctx, Font& _font, std::string_view _label, DirectX::XMFLOAT2* _value, const NumericEditorOptions<float>& _options, const PropertyLayout& _layout)
{
	if (_value == nullptr)
	{
		return false;
	}

	// XMFLOAT2/3/4はfloatが連続して並ぶ構造体なので、先頭アドレスを配列として扱える
	return PropertyVector(_ctx, _font, _label, &_value->x, 2, _options, _layout);
}

bool EditorUI::Property(Context& _ctx, Font& _font, std::string_view _label, DirectX::XMFLOAT3* _value, const NumericEditorOptions<float>& _options, const PropertyLayout& _layout)
{
	if (_value == nullptr)
	{
		return false;
	}

	return PropertyVector(_ctx, _font, _label, &_value->x, 3, _options, _layout);
}

bool EditorUI::Property(Context& _ctx, Font& _font, std::string_view _label, DirectX::XMFLOAT4* _value, const NumericEditorOptions<float>& _options, const PropertyLayout& _layout)
{
	if (_value == nullptr)
	{
		return false;
	}

	return PropertyVector(_ctx, _font, _label, &_value->x, 4, _options, _layout);
}
