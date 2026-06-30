// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Sampler.h"
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
Sampler::Sampler()
{ /* DO_NOTHING*/ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
Sampler::~Sampler() 
{ Term(); }

// -------------------------------------------------------------------------------
//		初期化
// -------------------------------------------------------------------------------
bool Sampler::Init(
	ID3D12Device*				_pDevice,
	DescriptorPool*				_pPool,
	const D3D12_SAMPLER_DESC&	_desc
)
{
	if (_pDevice == nullptr || _pPool == nullptr)
	{
		ELOG("Sampler::Init() Invalid argument");
		return false;
	}

	// 二重初期化を防ぐ
	assert(m_pHandle	== nullptr);
	assert(m_pPool		== nullptr);

	m_pPool = _pPool;
	m_pPool->AddRef();

	// DescriptorPoolからサンプラースロットを1つ借りる
	m_pHandle = m_pPool->AllocHandle();
	if (m_pHandle == nullptr)
	{
		ELOG("Sampler::Init() DescriptorPool slot full");
		return false;
	}

	// サンプラーをヒープに書き込む
	// samplerはバッファーではなくヒープスロットに直接CreateSampler()で書き込む
	_pDevice->CreateSampler(&_desc, m_pHandle->HandleCPU);

	// 設定を保持しておく（後から参照できるように）
	m_Desc = _desc;

	return true;
}

// -------------------------------------------------------------------------------
//		終了処理
// -------------------------------------------------------------------------------
void Sampler::Term()
{
	// DescriptorPoolにスロットを返却
	if (m_pPool != nullptr && m_pHandle != nullptr)
	{
		m_pPool->FreeHandle(m_pHandle);
		m_pHandle = nullptr;
	}

	if (m_pPool != nullptr)
	{
		m_pPool->Release();
		m_pPool = nullptr;
	}

	m_Desc = {};
}

// -------------------------------------------------------------------------------
//		GPUハンドルの取得
// -------------------------------------------------------------------------------
D3D12_GPU_DESCRIPTOR_HANDLE Sampler::GetHandleGPU() const
{
	if (m_pHandle == nullptr)
	{ return D3D12_GPU_DESCRIPTOR_HANDLE(0); }

	return m_pHandle->HandleGPU;
}

// -------------------------------------------------------------------------------
//		CPUハンドルの取得
// -------------------------------------------------------------------------------
D3D12_CPU_DESCRIPTOR_HANDLE Sampler::GetHandleCPU() const
{
	if (m_pHandle == nullptr) 
	{ return D3D12_CPU_DESCRIPTOR_HANDLE(0); }

	return m_pHandle->HandleCPU;
}

// -------------------------------------------------------------------------------
//		プリセット : バイリニア + ラップ
// -------------------------------------------------------------------------------
D3D12_SAMPLER_DESC Sampler::CreateLinearWrap()
{
	D3D12_SAMPLER_DESC desc = {};
	desc.Filter			= D3D12_FILTER_MIN_MAG_MIP_LINEAR;	// バイリニア（ミップもリニア間隔）
	desc.AddressU		= D3D12_TEXTURE_ADDRESS_MODE_WRAP;	// UVが0 ～ 1を超えたら繰り返す
	desc.AddressV		= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.AddressW		= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.MipLODBias		= 0.0f;								// ミップレベルのオフセット
	desc.MaxAnisotropy	= 1;								// 異方性なし
	desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;		// 比較サンプリングなし
	desc.MinLOD			= 0.0f;
	desc.MaxLOD			= D3D12_FLOAT32_MAX;				// 全ミップレベルを使用
	return desc;
}

// -------------------------------------------------------------------------------
//		プリセット : バイリニア + クランプ
// -------------------------------------------------------------------------------
D3D12_SAMPLER_DESC Sampler::CreateLinearClamp()
{
	D3D12_SAMPLER_DESC desc = {};
	desc.Filter			= D3D12_FILTER_MIN_MAG_MIP_LINEAR;	// バイリニア（ミップもリニア間隔）
	desc.AddressU		= D3D12_TEXTURE_ADDRESS_MODE_CLAMP;	// UVが0 ～ 1を超えたら端の色を使う
	desc.AddressV		= D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	desc.AddressW		= D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	desc.MipLODBias		= 0.0f;
	desc.MaxAnisotropy	= 1;
	desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	desc.MinLOD			= 0.0f;
	desc.MaxLOD			= D3D12_FLOAT32_MAX;
	return desc;
}

// -------------------------------------------------------------------------------
//		プリセット : バイリニア + ミラー
// -------------------------------------------------------------------------------
D3D12_SAMPLER_DESC Sampler::CreateLinearMirror()
{
	D3D12_SAMPLER_DESC desc = {};
	desc.Filter			= D3D12_FILTER_MIN_MAG_MIP_LINEAR;		// バイリニア（ミップもリニア間隔）
	desc.AddressU		= D3D12_TEXTURE_ADDRESS_MODE_MIRROR;	// 0→1→0→1と反転しながら繰り返す
	desc.AddressV		= D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	desc.AddressW		= D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	desc.MipLODBias		= 0.0f;
	desc.MaxAnisotropy	= 1;
	desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	desc.MinLOD			= 0.0f;
	desc.MaxLOD			= D3D12_FLOAT32_MAX;
	return desc;
}

// -------------------------------------------------------------------------------
//		プリセット : ポイント + ラップ
// -------------------------------------------------------------------------------
D3D12_SAMPLER_DESC Sampler::CreatePointWrap()
{
	D3D12_SAMPLER_DESC desc = {};
	desc.Filter			= D3D12_FILTER_MIN_MAG_MIP_POINT;	// 最近傍（補完なし）
	desc.AddressU		= D3D12_TEXTURE_ADDRESS_MODE_WRAP;	// UVが0 ～ 1を超えたら繰り返す
	desc.AddressV		= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.AddressW		= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.MipLODBias		= 0.0f;
	desc.MaxAnisotropy	= 1;
	desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	desc.MinLOD			= 0.0f;
	desc.MaxLOD			= D3D12_FLOAT32_MAX;
	return desc;
}

// -------------------------------------------------------------------------------
//		プリセット : ポイント + クランプ
// -------------------------------------------------------------------------------
D3D12_SAMPLER_DESC Sampler::CreatePointClamp()
{
	D3D12_SAMPLER_DESC desc = {};
	desc.Filter			= D3D12_FILTER_MIN_MAG_MIP_POINT;	// バイリニア（ミップもリニア間隔）
	desc.AddressU		= D3D12_TEXTURE_ADDRESS_MODE_CLAMP;	// UVが0 ～ 1を超えたら端の色を使う
	desc.AddressV		= D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	desc.AddressW		= D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	desc.MipLODBias		= 0.0f;
	desc.MaxAnisotropy	= 1;
	desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	desc.MinLOD			= 0.0f;
	desc.MaxLOD			= D3D12_FLOAT32_MAX;
	return desc;
}

// -------------------------------------------------------------------------------
//		プリセット : 異方性フィルタリング + ラップ
// -------------------------------------------------------------------------------
D3D12_SAMPLER_DESC Sampler::CreateAnisotropic(uint32_t _maxAnisotropy)
{
	// MaxAnisotropyは0～16の範囲に収める
	const auto aniso	= (_maxAnisotropy < 1) ? 1u
						: (_maxAnisotropy > 16) ? 16u
						: _maxAnisotropy;

	D3D12_SAMPLER_DESC desc = {};
	desc.Filter			= D3D12_FILTER_ANISOTROPIC;			// 異方性フィルタリング（斜め視点でも高品質）
	desc.AddressU		= D3D12_TEXTURE_ADDRESS_MODE_WRAP;	// UVが0 ～ 1を超えたら繰り返す
	desc.AddressV		= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.AddressW		= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.MipLODBias		= 0.0f;
	desc.MaxAnisotropy	= 1;
	desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	desc.MinLOD			= 0.0f;
	desc.MaxLOD			= D3D12_FLOAT32_MAX;
	return desc;
}

// -------------------------------------------------------------------------------
//		プリセット : シャドウマップ用比較サンプラー
// -------------------------------------------------------------------------------
D3D12_SAMPLER_DESC Sampler::CreateShadowMap()
{
	D3D12_SAMPLER_DESC desc = {};
	// COMPARISON_MIIN_MAG_LINEAR_MIP_POINT : 
	// ミップはポイントで選択しつつ、フィルタは比較後にバイリニア補完（PCF用）
	desc.Filter			= D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	desc.AddressU		= D3D12_TEXTURE_ADDRESS_MODE_BORDER;	// 範囲外はBorderColorを使う
	desc.AddressV		= D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	desc.AddressW		= D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	desc.MipLODBias		= 0.0f;
	desc.MaxAnisotropy	= 1;
	// LESS_EQUAL : シャドウマップの深度値 < サンプル値なら「影なし」と判定
	desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	desc.BorderColor[0] = 1.0f;	// 範囲外は「影なし」扱い（白 = 1.0）
	desc.BorderColor[1] = 1.0f;
	desc.BorderColor[2] = 1.0f;
	desc.BorderColor[3] = 1.0f;
	desc.MinLOD			= 0.0f;
	desc.MaxLOD			= 0.0f;	// シャドウマップはミップなし
	return desc;
}
