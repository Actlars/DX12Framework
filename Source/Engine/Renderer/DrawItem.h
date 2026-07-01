#pragma once

// -------------------------------------------------------------------------------
// 前方宣言
// -------------------------------------------------------------------------------
class Mesh;

// -------------------------------------------------------------------------------
// DrawItem	Struct
// 
// 概要 : 
//	1つの描画に必要な情報だけをまとめた構造体
// -------------------------------------------------------------------------------
struct DrawItem
{
	Mesh* pMesh = nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS	TransformCBAddress	= 0;
	D3D12_GPU_VIRTUAL_ADDRESS	MaterialCBAddress	= 0;
	D3D12_GPU_DESCRIPTOR_HANDLE TextureHandle		= {};
	bool						HasTexture			= false;

	// RootParameterのスロット番号
	uint32_t TransformSlot	= UINT32_MAX;
	uint32_t MaterialSlot	= UINT32_MAX;
	uint32_t TextureSlot	= UINT32_MAX;
};
