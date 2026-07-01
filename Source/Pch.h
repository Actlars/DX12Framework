#pragma once

// -------------------------------------------------------------------------------
// プリコンパイル済みヘッダー
// ここに書いたものは初回のみ解析（コンパイル）されるため、高速になる
// すべてのcppからインクルードされる必要がある
// -------------------------------------------------------------------------------

// -------------------------------------------------------------------------------
// 基本
// -------------------------------------------------------------------------------
#include <Windows.h>
#include <iostream>
#include <assert.h>

#include <wrl/client.h>

// -------------------------------------------------------------------------------
// STL
// -------------------------------------------------------------------------------
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <array>
#include <vector>
#include <stack>
#include <list>
#include <iterator>
#include <queue>
#include <algorithm>
#include <memory>
#include <random>
#include <fstream>
#include <sstream>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <future>
#include <filesystem>
#include <chrono>

// -------------------------------------------------------------------------------
// Microsoft
// -------------------------------------------------------------------------------
//#include <DX12Framework/Source/Framework/Utility/ComPtr/ComPtr.h>
#include "Engine/Utility/ComPtr/ComPtr.h"

// -------------------------------------------------------------------------------
// DirectX
// -------------------------------------------------------------------------------

#include <DirectXMath.h>

#include <DirectXTex.h>
#pragma comment(lib, "DirectXTex.lib")
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <include/d3dx12/d3dx12.h>

#pragma comment( lib, "d3d12.lib" )
#pragma comment( lib, "dxgi.lib" )
#pragma comment( lib, "d3dcompiler.lib" )
#pragma comment( lib, "dxguid.lib")

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <../External/nlohmann/json.hpp>
