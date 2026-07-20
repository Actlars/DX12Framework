#pragma once

// -------------------------------------------------------------------------------
// Includes
#include <Engine/RHI/Resource/DescriptorHeap/DescriptorPool/DescriptorPool.h>
#include <Engine/RHI/Resource/Texture/Texture.h>
#include <Engine/RHI/Core/Device/Device.h>
#include "../ResData.h"

// -------------------------------------------------------------------------------
// MaterialCB 構造体
// 
// 概要 : 
//	シェーダーに渡すマテリアルパラメータ
//	alignas（256）はDX12の定数バッファのアライメント要件（256バイト境界）
// -------------------------------------------------------------------------------
struct alignas(256) MaterialCB
{
	DirectX::XMFLOAT3	Diffuse		= { 0.5f,0.5f,0.5f };
	float				Alpha		= 1.0f;
	DirectX::XMFLOAT3	Specular	= { 0.0f,0.0f,0.0f };
	float				Shininess	= 0.0f;
	DirectX::XMFLOAT3	Emissive	= { 0.0f,0.0f,0.0f };
	float				Padding		= 0.0f;			//16バイトアライメント用パディング
};

// -------------------------------------------------------------------------------
// MaterialIndicesCB 構造体
// 
// 概要 : 
//	インデックス番号を詰める定数バッファ
//	テクスチャSRVを確保した後、そのハンドルのIndexを書き込む	
// -------------------------------------------------------------------------------
struct MaterialIndicesCB
{
	uint32_t DiffuseTextureIndex;
	uint32_t NormalTextureIndex;
	uint32_t SpecularTextureIndex;
	uint32_t Padding[3];
};

// -------------------------------------------------------------------------------
// Material class
// 
// 概要 : 
//	1マテリアル分のGPUリソースを管理するクラス
//	ResMaterialを受け取り、以下を生成する
//		- 定数バッファ（MaterialCB）
//		- テクスチャ（Diffuse / Normal / Specular / Shininess）
//		  存在しないテクスチャはダミー（1 * 1 白テクスチャ）で補完する
// 
// テクスチャスロット : 
//	TEXTURE_DIFFUSE		t0	ディヒューズカラー
//	TEXTURE_NORMAL		t1	法線マップ
//	TEXTURE_SPECULAR	t2	スペキュラーマップ
//	TEXTURE_SHININESS	t3	シャイネスマップ
// -------------------------------------------------------------------------------
class Material
{
public:

	// -------------------------------------------------------------------------------
	// テクスチャの種類
	// -------------------------------------------------------------------------------
	enum TextureType : uint32_t
	{
		TEXTURE_DIFFUSE		= 0,	// ディフューズマップとして利用
		TEXTURE_NORMAL		= 1,	// 法線マップとして利用
		TEXTURE_SPECULAR	= 2,	// スペキュラーマップとして利用
		TEXTURE_SHININESS	= 3,	// シャイネスマップとして利用
		TEXTURE_COUNT
	};

	// -------------------------------------------------------------------------------
	// @brief	コンストラクタ
	// -------------------------------------------------------------------------------
	Material();

	// -------------------------------------------------------------------------------
	// @brief	デストラクタ
	// -------------------------------------------------------------------------------
	~Material();

	// -------------------------------------------------------------------------------
	// @brief	初期化処理
	// 
	// @param[in]	_pDevice		デバイス
	// @param[in]	_pQueue		テクスチャアップロード用コマンドキュー
	// @param[in]	_pPool		ディスクリプタプール（CBV_UAV_SRV用のものを設定）
	// @param[in]	_resMat		ロード済みの生マテリアルデータ
	// @retval	true	初期化に成功
	// @retval	false	初期化に失敗
	// -------------------------------------------------------------------------------
	bool Init(
		RHI::Device*			_pRHIDevice,
		const ResMaterial&		_resMat);

	// -------------------------------------------------------------------------------
	// @brief	終了処理を行う
	// -------------------------------------------------------------------------------
	void Term();

	// -------------------------------------------------------------------------------
	// @brief	定数バッファのGPU仮想アドレスを返す
	//			SetGraphicsRootConstantBufferView()に渡す
	// -------------------------------------------------------------------------------
	D3D12_GPU_VIRTUAL_ADDRESS GetCBAddress() const;

	// -------------------------------------------------------------------------------
	// @brief	テクスチャのGPUハンドルを返す
	//			SetGraphicsRootConstantBufferView()に渡す
	// 
	// @param[in]	_type	テクスチャの種類
	// -------------------------------------------------------------------------------
	D3D12_GPU_DESCRIPTOR_HANDLE GetTextureHandle(TextureType _type) const;


	// -------------------------------------------------------------------------------
	// @brief	SRVハンドルからIndexを取得
	// -------------------------------------------------------------------------------
	uint32_t GetTextureIndex(TextureType _type) const;

	// -------------------------------------------------------------------------------
	// @brief	定数バッファのマップ済みポインタを返す（CPU側から書き換える場合）
	// -------------------------------------------------------------------------------
	MaterialCB* GetCBPtr() const;

private:

	// -------------------------------------------------------------------------------
	// private variables
	// -------------------------------------------------------------------------------
	ComPtr<ID3D12Resource>		m_pCB;						// 定数バッファ
	RHI::Device*				m_pDevice		= nullptr;	// ダミーテクスチャ参照用のデバイス
	RHI::DescriptorHandle*		m_pCBHandle		= nullptr;	// CBVハンドル
	RHI::DescriptorPool*		m_pPool			= nullptr;	// プール
	MaterialCB*					m_pMappedPtr	= nullptr;	// Map済みポインタ
	
	RHI::Texture	m_Textures[TEXTURE_COUNT];			// テクスチャ[4]
	bool			m_HasTexture[TEXTURE_COUNT] = {};	// テクスチャが存在するか


	Material		(const Material&) = delete;
	void operator = (const Material&) = delete;
};