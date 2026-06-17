#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif//defined(DEBUG) || defined(_DEBUG)
#pragma once

// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Application/Application.h"

// -------------------------------------------------------------------------------
// メインエントリーポイント
// -------------------------------------------------------------------------------
int wmain(int argc, wchar_t** argv, wchar_t** evnp)
{
#if defined(DEBUG) || defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif//defined(DEBUG) || defined(_DEBUG)

	// アプリケーションを実行
	Application application(960, 540);
	application.Run();

	return 0;
}
