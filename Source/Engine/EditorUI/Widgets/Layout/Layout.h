#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Context.h>

namespace EditorUI
{
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
		const float lineY = _frame->CursorPos.y;
		Rect2D bounds = MakeRect({ _frame->CursorPos.x, lineY }, _size);

		// SameLine()が直前のウィジェットの右端を参照できるよう、配置結果を記録
		_frame->LastItemBound	= bounds;
		_frame->LineY			= lineY;

		// デフォルトでは次の行の左端(ContextOrigin.x)へ戻る。SameLineが呼ばれれば上書きされる
		_frame->CursorPos = { _frame->ContentOrigin.x, lineY + _size.y + _itemSpacing };

		return bounds;
	}

	// -------------------------------------------------------------------------------
	// @brief	直前のウィジェットの右側に、次のウィジェットを続けて配置する
	// 
	// @param[in]	_frame			対象のウィンドウの1フレーム分の作業データ
	// @param[in]	_itemSpacing	直前のウィジェットとの間隔
	// -------------------------------------------------------------------------------
	inline void SameLine(WindowFrame* _frame, float _itemSpacing) 
	{ _frame->CursorPos = { _frame->LastItemBound.Max.x + _itemSpacing, _frame->LineY }; }

	// -------------------------------------------------------------------------------
	// @brief	縦方向に空白を1行分空ける
	// 
	// @param[in]	_frame	対象のウィンドウの1フレーム分の作業データ
	// @param[in]	_amount	空ける高さ
	// -------------------------------------------------------------------------------
	inline void Spacing(WindowFrame* _frame, float _amount) 
	{ _frame->CursorPos.y += _amount; }

}
