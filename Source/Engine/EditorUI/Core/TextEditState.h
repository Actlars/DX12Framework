#pragma once
#include <Engine/EditorUI/Core/Id.h>

namespace EditorUI
{
	// -------------------------------------------------------------------------------
		// TextEditState struct
		//
		// 概要 :
		//	キーボードで編集中のテキストを、フレームをまたいで保持するための状態
		//
		//	即時モードのウィジェットは毎フレーム作り直されるため、編集途中の文字列や
		//	キャレット位置を自分で持てない。一方で「同時に編集できるウィジェットは
		//	常に1つだけ」なので、Contextがこの1組だけを持てば足りる
		//
		//	文字列をワイド文字で持つのは、キャレット移動や選択範囲を「文字単位」で
		//	扱うため。UTF-8のまま扱うとマルチバイト文字の途中を指してしまう
		// -------------------------------------------------------------------------------
	struct TextEditState
	{
		Id				Widget			= 0;		// 編集対象のウィジェットId。0は「編集していない」
		std::wstring	Text;						// 編集中の文字列
		int				CursorIndex		= 0;		// キャレット位置(文字単位)
		int				SelectionAnchor = 0;		// 選択の始点。CursorIndexと等しければ選択なし
		float			ScrollX			= 0.0f;		// 入力欄からあふれた分の横スクロール量
		bool			Canceled		= false;	// Escapeで破棄された
		bool			Alive			= false;	// 今フレームも所有ウィジェットから参照されたか

		bool HasSelection()		const { return CursorIndex != SelectionAnchor; }
		int	 SelectionBegin()	const { return (std::min)(CursorIndex, SelectionAnchor); }
		int	 SelectionEnd()		const { return (std::max)(CursorIndex, SelectionAnchor); }

		// 選択範囲を解除し、キャレットだけを残す
		void ClearSelection() { SelectionAnchor = CursorIndex; }

		void Reset() { *this = TextEditState{}; }
	};
}
