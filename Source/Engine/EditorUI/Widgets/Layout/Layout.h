#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Context/Context.h>

// -------------------------------------------------------------------------------
// Layout
//
// 概要 :
//	ウィジェットを縦一列に積んでいくための、カーソル位置の管理を担う
//
//	座標系について
//		仮想座標 : スクロール量を含まない、コンテンツ全体の中での位置
//		           ウィンドウがどれだけスクロールされていても値は変わらない
//		スクリーン座標 : 仮想座標からスクロール量を引いた、実際に描画する位置
//
//	CursorPosとLineYは仮想座標、PlaceWidgetが返す矩形はスクリーン座標で扱う
//	この境界をPlaceWidgetの中の1か所だけに閉じ込めることで、
//	各ウィジェットはスクロールの存在を意識せずに実装できる
// -------------------------------------------------------------------------------
namespace EditorUI
{
	// -------------------------------------------------------------------------------
	// @brief	コンテンツを置ける横幅（ウィンドウ幅から左右のパディングを引いたもの）を返す
	//
	// @param[in]	_frame	対象のウィンドウの1フレーム分の作業データ
	// @param[in]	_style	参照するスタイル
	// @return	コンテンツ領域の横幅。負にはならない
	// -------------------------------------------------------------------------------
	inline float GetContentWidth(const WindowFrame* _frame, const Style& _style)
	{
		if (_frame == nullptr) { return 0.0f; }

		// スクロールバーは常時表示ではないため、幅の計算からは常に除外する
		// 表示・非表示で行幅が変わると、ウィジェットが左右に揺れて見えてしまう
		// インデント中はその分だけ行が短くなる
		return (std::max)(0.0f, _frame->WindowRect.Width() - _style.WindowPadding * 2.0f - _frame->IndentX);
	}

	// -------------------------------------------------------------------------------
	// @brief	以降の行の左端を1段深くする（ツリー表示用）
	//
	// @param[in]	_frame	対象のウィンドウの1フレーム分の作業データ
	// @param[in]	_amount	インデント量
	// -------------------------------------------------------------------------------
	inline void Indent(WindowFrame* _frame, float _amount)
	{
		if (_frame == nullptr) { return; }

		_frame->IndentX		+= _amount;
		_frame->CursorPos.x	= _frame->ContentOrigin.x + _frame->IndentX;
	}

	// -------------------------------------------------------------------------------
	// @brief	Indentで深くした分を1段戻す
	// -------------------------------------------------------------------------------
	inline void Unindent(WindowFrame* _frame, float _amount)
	{
		if (_frame == nullptr) { return; }

		_frame->IndentX		= (std::max)(0.0f, _frame->IndentX - _amount);
		_frame->CursorPos.x	= _frame->ContentOrigin.x + _frame->IndentX;
	}

	// -------------------------------------------------------------------------------
	// @brief	次に置くウィジェット1つだけの横幅を指定する
	//
	//	消費されると自動で解除されるため、Popに相当する呼び出しは不要
	//
	// @param[in]	_frame	対象のウィンドウの1フレーム分の作業データ
	// @param[in]	_width	横幅。0以下を渡すとコンテンツ領域いっぱいに広がる
	// -------------------------------------------------------------------------------
	inline void SetNextItemWidth(WindowFrame* _frame, float _width)
	{
		if (_frame == nullptr) { return; }
		_frame->NextItemWidth = _width;
	}

	// -------------------------------------------------------------------------------
	// @brief	SetNextItemWidthで予約された幅を取り出し、予約を解除する
	//
	// @param[in]	_frame			対象のウィンドウの1フレーム分の作業データ
	// @param[in]	_defaultWidth	予約がなかった場合に使う幅
	// @return	このウィジェットが使うべき横幅
	// -------------------------------------------------------------------------------
	inline float ConsumeNextItemWidth(WindowFrame* _frame, float _defaultWidth)
	{
		if (_frame == nullptr) { return _defaultWidth; }

		const float width = (_frame->NextItemWidth > 0.0f) ? _frame->NextItemWidth : _defaultWidth;
		_frame->NextItemWidth = 0.0f;	// 1回で使い切る
		return width;
	}

	// -------------------------------------------------------------------------------
	// @brief	指定サイズのウィジェットを現在のカーソル位置に配置し、カーソルを次の行へ進める
	//
	// すべてのウィジェットはこの関数を経由してレイアウトに参加する
	//
	// @param[in]	_frame			配置対象のウィンドウの1フレーム分の作業データ
	// @param[in]	_size			ウィジェットのサイズ
	// @param[in]	_itemSpacing	次のウィジェットとの間隔
	// @return	配置されたウィジェットのスクリーン座標での矩形
	// -------------------------------------------------------------------------------
	inline Rect2D PlaceWidget(WindowFrame* _frame, const DirectX::XMFLOAT2& _size, float _itemSpacing)
	{
		// lineYはスクロールの影響を受けないコンテンツ全体の中での仮想位置
		// drawYは実際に画面へ描く位置で、ここでだけScrollを差し引く
		const float lineY = _frame->CursorPos.y;
		const float drawY = lineY - _frame->pState->Scroll.y;

		const Rect2D bounds = MakeRect({ _frame->CursorPos.x, drawY }, _size);

		// SameLine()が直前のウィジェットの右端を参照できるよう、配置結果を記録する
		// 記録をこの1か所に集約しているので、各ウィジェットは後始末を書かなくてよい
		_frame->LastItemRect	= bounds;
		_frame->LineY			= lineY;	// SameLineも仮想座標系のまま扱う
		_frame->HasLastItem		= true;

		// デフォルトでは次の行の左端(ContentOrigin.x + インデント量)へ戻る
		// SameLineが呼ばれれば上書きされる
		_frame->CursorPos = { _frame->ContentOrigin.x + _frame->IndentX, lineY + _size.y + _itemSpacing };

		// このフレームで到達した最大の高さを記録しておく
		_frame->ContentHeight = (std::max)(_frame->ContentHeight, (lineY + _size.y) - _frame->ContentOrigin.y);

		// 到達した最大の幅も記録する
		// ポップアップのように「中身に合わせてサイズを決める」ウィンドウが参照する
		_frame->ContentWidthUsed = (std::max)(_frame->ContentWidthUsed, bounds.Max.x - _frame->ContentOrigin.x);

		return bounds;
	}

	// -------------------------------------------------------------------------------
	// @brief	直前のウィジェットの右側に、次のウィジェットを続けて配置する
	//
	// @param[in]	_frame			対象のウィンドウの1フレーム分の作業データ
	// @param[in]	_itemSpacing	直前のウィジェットとの間隔
	// -------------------------------------------------------------------------------
	inline void SameLine(WindowFrame* _frame, float _itemSpacing)
	{
		if (_frame == nullptr || !_frame->HasLastItem) { return; }

		_frame->CursorPos.x = _frame->LastItemRect.Max.x + _itemSpacing;

		// 縦位置はLastItemRect(スクリーン座標)ではなくLineY(仮想座標)から戻す
		// スクリーン座標を入れてしまうと、PlaceWidgetで再度Scrollが引かれて
		// スクロール量が二重に適用されてしまう
		_frame->CursorPos.y = _frame->LineY;
	}

	// -------------------------------------------------------------------------------
	// @brief	縦方向に空白を1行分空ける
	//
	// @param[in]	_frame	対象のウィンドウの1フレーム分の作業データ
	// @param[in]	_amount	空ける高さ
	// -------------------------------------------------------------------------------
	inline void Spacing(WindowFrame* _frame, float _amount)
	{
		if (_frame == nullptr) { return; }
		_frame->CursorPos.y += _amount;
	}
}
