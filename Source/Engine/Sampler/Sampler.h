#pragma once

// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/Pool/DescriptorPool/DescriptorPool.h>

// -------------------------------------------------------------------------------
// Sampler class
// 
// 概要 : 
//	テクスチャサンプリングの設定（フィルタリング・アドレスモード等）を
//	GPUリソースとして管理するクラス。
//	DescriptorPool（POOL_TYPE_SMP）からスロットを借りて
//	CreateSampler()でヒープに書き込む
// 
//	マテリアル・パスごとに実行差し替え可能にする
// 
// よく使うサンプラーはプリセットメソッドで即生成可能にしておく
//		Sampler::CreateLinearWrap()		バイリニア + テクスチャ繰り返し（最も汎用的かも）
//		Sampler::CreateLinearClamp()	バイリニア + 端でクランプ（UIやポストエフェクト）
//		Sampler::CreatePointWrap()		ポイント   + テクスチャ繰り返し（ドット絵）
//		Sampler::CreatePointClamp()		ポイント   + 端でクランプ（GBufferサンプリング等）
//		Sampler::CreateAnisotropic()	異方性	   + 繰り返し（斜め見えのテクスチャ）
//		Sampler::CreateShadowMap()		比較サンプラー（PCF・シャドウマップ用）
// 
// 使い方 : 
//	Sampler sampler;
//	sampler.Init(pDevice, pSmpPool, Sampler::CreateLinearWrap());
// 
//	毎フレームのバインド
//	pCmd->SetGraphicsRootDescriptorTable(SLOT_SAMPLER, sampler, GetHandleGPU());
//
//	RootSignature側の設定（DescriptorRange）
//	D3D12_DESCRIPTOR_RANGE range;
//	range.RangeType				= D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
//	range.NumDescriptors		= 1;
//	range.BaseShaderRegister	= 0;	//register(s0)
// -------------------------------------------------------------------------------
class Sampler
{
public:

	// -------------------------------------------------------------------------------
	// コンストラクタ
	// -------------------------------------------------------------------------------
	Sampler();

	// -------------------------------------------------------------------------------
	// デストラクタ
	// -------------------------------------------------------------------------------
	~Sampler();

	// -------------------------------------------------------------------------------
	// @brief	初期化
	// 
	// @param[in]	_pDevice	デバイス
	// @param[in]	_pPool		POOL_TYPE_SMPのDescriptorPool
	// @param[in]	_desc		サンプラーの設定（プリセットメソッドで生成）
	// @retval	true	初期化成功
	// @retval	false	初期化失敗
	// -------------------------------------------------------------------------------
	bool Init(ID3D12Device* _pDevice, DescriptorPool* _pPool, const D3D12_SAMPLER_DESC& _desc);

	// -------------------------------------------------------------------------------
	// @brief	終了処理
	// -------------------------------------------------------------------------------
	void Term();

	// -------------------------------------------------------------------------------
	// @brief	シェーダーにバインドするGPUハンドルを返す
	//			SetGraphicsRootDescriptorTable()に渡す
	// -------------------------------------------------------------------------------
	D3D12_GPU_DESCRIPTOR_HANDLE GetHandleGPU() const;

	// -------------------------------------------------------------------------------
	// @brief	CPUハンドルを返す（デバッグ用途）	
	// -------------------------------------------------------------------------------
	D3D12_CPU_DESCRIPTOR_HANDLE GetHandleCPU() const;

	// -------------------------------------------------------------------------------
	// @brief	現在の D3D12_SAMPLER_DESC を返す
	// -------------------------------------------------------------------------------
	const D3D12_SAMPLER_DESC& GetDesc() const;

	// -------------------------------------------------------------------------------
	// @brief	初期化済みかどうかを返す
	// -------------------------------------------------------------------------------
	bool IsValid()	const;

	// -------------------------------------------------------------------------------
	// プリセット生成メソッド
	// 
	// よく使うサンプラー設定を返す。Init()の第3引数に渡して使う
	// -------------------------------------------------------------------------------
	
	// @brief	バイリニア + ラップ（キャラクター・背景テクスチャ全般）
	static D3D12_SAMPLER_DESC CreateLinearWrap();

	// @brief	バイリニア + クランプ（UI・ポストエフェクト・RenderTarget サンプリング）
	static D3D12_SAMPLER_DESC CreateLinearClamp();

	// @brief	バイリニア + ミラー（タイル模様を左右反転しながら繰り返す）
	static D3D12_SAMPLER_DESC CreateLinearMirror();

	// @brief	ポイント + ラップ（ドット絵・ピクセルアートスタイル）
	static D3D12_SAMPLER_DESC CreatePointWrap();

	// @brief	ポイント + クランプ（GBuffer・深度バッファの読み取り）
	static D3D12_SAMPLER_DESC CreatePointClamp();

	// @brief	異方性フィルタリング + ラップ（地面・壁テクスチャ向け）
	// @param[in]	_maxAnisotropy	異方性レベル（1 ～ 16 大きいほど高品質だが重い）
	static D3D12_SAMPLER_DESC CreateAnisotropic(uint32_t _maxAnisotropy = 8);

	// @brief	比較サンプラー（PCF・シャドウマップ用）、SampleCmpLevelZero()で使用する
	static D3D12_SAMPLER_DESC CreateShadowMap();

private:

	// -------------------------------------------------------------------------------
	// private variables
	// -------------------------------------------------------------------------------
	DescriptorHandle*	m_pHandle	= nullptr;	// プールから借りたハンドル
	DescriptorPool*		m_pPool		= nullptr;	// プール（所有権なし）
	D3D12_SAMPLER_DESC	m_Desc		= {};		// サンプラー設定のコピー

	Sampler			(const Sampler&) = delete;
	void operator = (const Sampler&) = delete;
};











