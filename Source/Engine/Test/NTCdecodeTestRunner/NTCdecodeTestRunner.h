#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/RHI/Pipeline/RootSignature/RootSignatureLayout/RootSignatureLayout.h>
#include <Engine/RHI/Resource/Buffer/Buffer.h>

namespace RHI { class  Device; }

// -------------------------------------------------------------------------------
// NTCdecodeTestRunner class
// 
// 固定の重みの2層MLP（4→4→3）をGPUで推論し、CPUの期待値と比較検証する
// 
// 構成 : 潜在ベクトル4要素 → 隠れ層4要素（ReLU） → 出力3要素（線形）
// Layer0 : 4x4 単位行列 + ReLU → 入力そのまま[1,2,3,4]
// Layer1 : 3x4 合計パターン行列 →	出力[10,6,4]
// -------------------------------------------------------------------------------
class NTCDecodeTestRunner
{
public:

	static constexpr uint32_t kInputDim = 4;	//潜在ベクトルの要素数
	static constexpr uint32_t kHiddenDim = 4;	// 隠れ層の要素数
	static constexpr uint32_t kOutputDim = 3;	// 出力（デコード結果）の要素数

	bool Run(RHI::Device* _pDevice);

private:

	RHI::RootSignatureLayout m_RootSignatureLayout;

	RHI::Buffer m_InputVector;			// Upload : 入力潜在ベクトル
	RHI::Buffer m_WeightBuffer;			// Upload : Layer0 / Layer1 重み + バイアス連結(float)
	RHI::Buffer m_OutputBufferGPU;		// Default + UAV
	RHI::Buffer m_OutputBufferReadback;	// Readback

};