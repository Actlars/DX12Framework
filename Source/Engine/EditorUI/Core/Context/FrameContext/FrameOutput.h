#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/DrawList/DrawList.h>

namespace EditorUI
{
	// -------------------------------------------------------------------------------
	// FrameOutput struct
	// 
	// 概要 : 
	//	最終描画出力
	// -------------------------------------------------------------------------------
	struct FrameOutput
	{
		// Rendererが背面から前面の順に消費するWindow単位のDrawList
		std::vector<const DrawList*> WindowDrawLists;
	};
}
