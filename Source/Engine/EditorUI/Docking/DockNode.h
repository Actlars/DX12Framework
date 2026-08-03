#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Id.h>
#include <Engine/EditorUI/Core/Types.h>

namespace EditorUI
{
	// -------------------------------------------------------------------------------
	// DockSplitDir enum class
	// 
	// 概要 : 
	//	ウィンドウをドロップした位置が、既存のLeafに対してどの位置にあるかを表す
	//	Centerはタブとして同じLeafに追加することを意味する
	// -------------------------------------------------------------------------------
	enum class DockSplitDir
	{
		None,
		Left,
		Right,
		Top,
		Bottom,
		Center,
	};

	// -------------------------------------------------------------------------------
	// DockNode struct
	// 
	// 概要 : 
	//	Dock木の1ノード。IsLeaf == trueなら実際にウィンドウを収める区画、
	//	falseなら領域を２つに割って子ノードを持つ区画のどちらかとして機能
	// -------------------------------------------------------------------------------
	struct DockNode
	{
		int Id		= -1;
		int Parent	= -1;	// ルートノードは-1

		bool IsLeaf = true;

		// Leafノードの時だけ使うデータ
		std::vector<EditorUI::Id> Windows;	// このLeafにドッキングされているウィンドウ
		int ActiveTabIndex = 0;				// 今表示中のタブ

		// Splitノードの時だけ使うデータ
		bool	SplitHorizontal	= true;	// true : 左右分割、false : 上下分割
		float	SplitRatio		= 0.5f;	// ChildAが占める割合(0.0～1.0)
		int		ChildA			= -1;
		int		ChildB			= -1;

		// 全ノード共通 : 毎フレーム親から再計算されるスクリーン座標での領域
		Rect2D Bounds{};
	};
}
