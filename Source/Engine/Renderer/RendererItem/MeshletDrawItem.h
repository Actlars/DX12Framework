#pragma once

// -------------------------------------------------------------------------------
// 前方宣言
// -------------------------------------------------------------------------------
class MeshletResource;

// -------------------------------------------------------------------------------
// MeshletDrawItem	Struct
// 
// 概要 : 
//	メッシュレット描画1個分に必要な情報だけをまとめた構造体
//	DrawItemのMeshShaderパイプライン版
// -------------------------------------------------------------------------------
struct MeshletDrawItem
{
	MeshletResource*			pMesh				= nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS	TransformCBAddress	= 0;
	uint32_t					DiffuseTextureIndex = 0;

	// RootParameterのスロット番号
	uint32_t TransformSlot		= UINT32_MAX;	// CBV
	uint32_t TextureIndexSlot	= UINT32_MAX;	// RootConstants（"TextureIndex"）
};
