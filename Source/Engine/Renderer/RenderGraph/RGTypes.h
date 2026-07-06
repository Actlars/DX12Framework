#pragma once

namespace RG
{
	// -------------------------------------------------------------------------------
	// リソースハンドル
	// 
	// 概要 : 
	//	RenderGraphの外にある実リソースをパス側が直接ID3D12Resourceとして利用するのではなく
	//	ハンドル（ただの整数）で扱うことで、パスの宣言をDX12型に依存しなくさせる
	// -------------------------------------------------------------------------------

	using	Handle = uint32_t;
	constexpr	Handle InvalidHandle = UINT32_MAX;

	// -------------------------------------------------------------------------------
	// TransientResourceDesc
	// 
	// 概要 : 
	//	パスがCreateTransientで生成を要求する際に渡すリソースの仕様
	// -------------------------------------------------------------------------------
	struct TransientResourceDesc
	{
		uint32_t	Width			= 0;
		uint32_t	Height			= 0;
		DXGI_FORMAT	Format			= DXGI_FORMAT_R8G8B8A8_UNORM;
		float		ClearColor[4]	= { 0.0f,0.0f,0.0f,0.0f };
	};
}
