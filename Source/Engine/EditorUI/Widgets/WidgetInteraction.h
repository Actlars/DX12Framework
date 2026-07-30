#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Context.h>

namespace EditorUI
{
	// -------------------------------------------------------------------------------
	// InteractionState struct
	// 
	// 概要 : 
	//	1つのウィジェットに対する、このフレームでの入力状態をまとめたもの
	// -------------------------------------------------------------------------------
	struct InteractionState
	{
		bool Hovered	= false;	// マウスが矩形の上にあるか
		bool Held		= false;	// マウスが矩形の上で押され続けているか
		bool Clicked	= false;	// このフレームでクリックが確定したか
	};

	// -------------------------------------------------------------------------------
	// @brief	矩形1つ分の当たり判定をまとめて計算する
	// 
	// @param[in]	_ctx			当たり判定に使う入力情報を持つContext
	// @param[in]	_bounds			判定対象の矩形
	// @param[in]	_mouseButton	判定に使うマウスボタン
	// -------------------------------------------------------------------------------
	inline InteractionState UpdateInteraction(Context& _ctx, const Rect2D& _bounds, int _mouseButton = 0)
	{
		InteractionState state;
		state.Hovered	= _bounds.Contains(_ctx.GetMousePos());
		state.Held		= state.Hovered && _ctx.IsMouseDown(_mouseButton);
		state.Clicked	= state.Hovered && _ctx.IsMouseClicked(_mouseButton);
		return state;
	}
}
