#pragma once

namespace EditorUI
{
	enum class MouseButton
	{
		Mouse_Left		= 0,
		Mouse_Right		= 1,
		Mouse_Middle	= 2,
		Mouse_Max		= 3,
	};

	// -------------------------------------------------------------------------------
	// InputState struct
	// 
	// 概要 : 
	//	プラットフォーム非依存の入力スナップショット
	// -------------------------------------------------------------------------------
	struct InputState
	{
		// スクリーン座標でのマウスの位置。ウィンドウ外や無効値のときは(-1,-1)を初期値にしている
		DirectX::XMFLOAT2	MousePos{ -1.0f,-1.0f };
		// 現在の押下状態のみを持つ。0:左, 1:右, 2:中
		bool				MouseDown[3]	= { false,false,false };
		// ホイールの回転量（１フレーム当たりの増分）
		float				MouseWheel		= 0.0f;
	};

}
