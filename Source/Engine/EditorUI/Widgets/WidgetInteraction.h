#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Context/Context.h>

// -------------------------------------------------------------------------------
// WidgetInteraction
//
// 概要 :
//	「矩形1つ + Id 1つ」に対するマウス操作の判定を、全ウィジェット共通の形にまとめる
//
//	ここを経由することで、どのウィジェットも次の性質を自動的に満たす
//		- 押している間はそのウィジェットが入力を占有する（ActiveId）
//		- 手前のウィンドウに隠れているウィジェットは反応しない
//		- 押したまま枠外へ出ても操作が途切れず、枠外で離せばキャンセルになる
// -------------------------------------------------------------------------------
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
		bool Hovered		= false;	// マウスが矩形の上にあり、かつ他のウィジェットに邪魔されていない
		bool Held			= false;	// このウィジェットが入力を占有し、ボタンが押され続けている
		bool Clicked		= false;	// 矩形の上で押して、矩形の上で離した（クリック成立）
		bool Pressed		= false;	// このフレームで押し込まれた瞬間
		bool Released		= false;	// このフレームでボタンが離された瞬間
		bool Deactivated	= false;	// このフレームで入力の占有を終えた
	};

	// -------------------------------------------------------------------------------
	// @brief	矩形1つ分の当たり判定をまとめて計算する
	//
	//	Clickedを「押した瞬間」ではなく「矩形の上で離した瞬間」としているのは、
	//	押したあとに矩形の外へマウスを逃がして操作を取り消せるようにするため
	//	一般的なGUIと同じ操作感になる
	//
	// @param[in]	_ctx			当たり判定に使う入力情報を持つContext
	// @param[in]	_id				このウィジェットのId（入力の占有に使う）
	// @param[in]	_bounds			判定対象の矩形
	// @param[in]	_mouseButton	判定に使うマウスボタン
	// @return	このフレームでの入力状態
	// -------------------------------------------------------------------------------
	inline InteractionState UpdateInteraction(
		Context&		_ctx,
		Id				_id,
		const Rect2D&	_bounds,
		MouseButton		_mouseButton = MouseButton::Mouse_Left)
	{
		InteractionState state;

		const bool isActive		= _ctx.IsActiveId(_id);
		const bool mouseInside	= _bounds.Contains(_ctx.GetMousePos());

		// 他のウィジェットが操作中、または手前の別ウィンドウに覆われている場合は反応しない
		// ただし自分が操作中なら、覆われていようと入力を受け取り続ける
		const bool canInteract = isActive ||
			(!_ctx.IsAnyItemActive() && _ctx.IsCurrentWindowHovered());

		state.Hovered = mouseInside && canInteract;

		if (state.Hovered)
		{
			_ctx.SetHoveredId(_id);
		}

		// 押し込み : ここで入力の占有を開始する
		if (state.Hovered && _ctx.IsMouseClicked(_mouseButton) && !isActive)
		{
			_ctx.SetActive(_id);
			state.Pressed = true;
		}

		if (!_ctx.IsActiveId(_id))
		{
			return state;	// 占有していないウィジェットは、ホバー以外の情報を持たない
		}

		// 占有中であることを申告する。これを怠るとContextが「消えた」と判断して解放する
		_ctx.KeepActiveIdAlive(_id);

		state.Held		= _ctx.IsMouseDown(_mouseButton);
		state.Released	= _ctx.IsMouseReleased(_mouseButton);

		if (!state.Held)
		{
			// 矩形の中で離せばクリック成立、外へ逃がしていればキャンセル
			state.Clicked		= mouseInside;
			state.Deactivated	= true;
			_ctx.ClearActiveId(_id);
		}

		return state;
	}
}
