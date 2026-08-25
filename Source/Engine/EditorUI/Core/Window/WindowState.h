#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Id.h>

namespace EditorUI
{
	// -------------------------------------------------------------------------------
	// WindowState struct
	// 
	// 概要 : WindowManagerが所有するフレームをまたぐ永続状態
	// -------------------------------------------------------------------------------
	struct WindowState
	{
		Id WindowId = 0;

		// 画面に表示する名前。タイトルバーとドックタブのラベルに使う
		// Idはハッシュ値で元の文字列に戻せないため、ここに実体を持っておく
		std::string Title;

		DirectX::XMFLOAT2 Position{ 60.0f,60.0f };
		DirectX::XMFLOAT2 Size{ 320.0f,240.0f };
		DirectX::XMFLOAT2 Scroll{ 0.0f,0.0f };
		bool	Collapsed	= false;
		bool	Active		= false;	// 今フレームでBeginWindowされたか
		int		DockNodeId	= -1;		// 予約 : Docking実装時にドック先ノードIDを入れる
		float	MaxScrollY	= 0.0f;		// 前フレーム終了時点で確定したスクロール可能な最大量

		// 呼び出し側がbool*を渡していた場合の、×ボタンによる閉じる要求
		// EditorUI側からアプリの変数を直接触らないよう、要求だけを立てて次のBeginWindowで反映する
		bool	CloseRequested = false;
	};
}
