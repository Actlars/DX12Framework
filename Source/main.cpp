#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif//defined(DEBUG) || defined(_DEBUG)

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 721; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

//namespace
//{
//	// グローバル初期化のタイミングでログを出すためのヘルパー
//	struct ExperimentalFeatureLogger
//	{
//		ExperimentalFeatureLogger()
//		{
//			UUID experimentalFeatures[] = { D3D12ExperimentalShaderModels };
//			HRESULT hr = D3D12EnableExperimentalFeatures(
//				1, experimentalFeatures, nullptr, nullptr);
//
//			wchar_t buf[256];
//			swprintf_s(buf, L"[main.cpp] D3D12EnableExperimentalFeatures hr = 0x%08X\n", static_cast<unsigned int>(hr));
//			OutputDebugStringW(buf);
//
//			if (hr == S_OK)
//			{
//				OutputDebugStringW(L"[main.cpp] -> S_OK : experimental shader models enabled\n");
//			}
//			else if (hr == E_NOINTERFACE)
//			{
//				OutputDebugStringW(L"[main.cpp] -> E_NOINTERFACE : this SDK no longer allows enabling shader models via this API (expected on recent Agility SDK)\n");
//			}
//			else if (hr == E_INVALIDARG)
//			{
//				OutputDebugStringW(L"[main.cpp] -> E_INVALIDARG : Developer Mode is likely NOT enabled on this machine\n");
//			}
//			else
//			{
//				OutputDebugStringW(L"[main.cpp] -> unknown HRESULT\n");
//			}
//		}
//	} g_experimentalFeatureLogger;
//}

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

	printf(
		"[Agility] D3D12SDKVersion = %u\n",
		D3D12SDKVersion
	);

	printf(
		"[Agility] D3D12SDKPath = %s\n",
		D3D12SDKPath
	);

	// アプリケーションを実行
	Application application(960, 540);
	application.Run();

	return 0;
}
