#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/RHI/Pipeline/RootSignature/RootSignatureLayout/RootSignatureLayout.h>
#include <Engine/RHI/Resource/Buffer/Buffer.h>

namespace RHI { class Device; }

// -------------------------------------------------------------------------------
// CoopVecMatMulTestRunner class
// 
// 目的 : 
//	CoopVecMatMul（4 * 4行列 * 4要素ベクトル）をGPUで実行し
//	CPU側で計算した期待値を（単純な行列 * ベクトルの手計算）と比較検証する
// -------------------------------------------------------------------------------
class CoopVecMatMulTestRunner
{
public:

	static constexpr uint32_t kDim = 4;	// 行列の次元（4 * 4）、ベクトルの要素数

	bool Run(RHI::Device* _pDevice);

private:

	RHI::RootSignatureLayout m_RootSignatureLayout;

	RHI::Buffer m_InputVector;				// Upload :	入力ベクトル（4要素）
	RHI::Buffer m_WeightMatrix;				// Upload : 重み行列（4 * 4 = 16要素）
	RHI::Buffer m_OutputBufferGPU;			// Default + UAV
	RHI::Buffer m_OutputBufferReadback;		// Readback

};
