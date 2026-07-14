#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/RHI/Pipeline/RootSignature/RootSignatureLayout/RootSignatureLayout.h>
#include <Engine/RHI/Resource/Buffer/Buffer.h>

namespace RHI { class Device; }

// -------------------------------------------------------------------------------
// CoopVecTestRunner class
// 
// 概要 : 
//	Slangで書いたCoopVecTest.slangを実際にGPUへDispatchし、結果をCPUへ読み戻して検証するためのテストクラス
// 
//	[numthreads(4,1,1)]のシェーダーなので、1回のDispatch(1,1,1)で
//	16スレッド(4x4)が起動し、各スレッドが32要素のCoopVecを1本ずつ処理する
//	→ outputBufferの要素数は 16 * 32 = 512 個の in32_t
// -------------------------------------------------------------------------------
class CoopVecTestRunner
{
public:

	// 1スレッドあたりの要素数（シェーダー側のCoopVec<int32_t, 32)と一致させる)
	static constexpr uint32_t kElementsPerThread = 32;
	// numthreads(4,4,1)] = 4 * 4 * 1 = 16スレッド
	static constexpr uint32_t kThreadCount = 4 * 4 * 1;
	static constexpr uint32_t kTotalElements = kElementsPerThread * kThreadCount;

	bool Run(RHI::Device* _pDevice);

private:

	RHI::RootSignatureLayout m_RootSignatureLayout;

	RHI::Buffer m_InputBuffer1;			// Upload : CPU→GPU
	RHI::Buffer m_InputBuffer2;			// Upload : CPU→GPU
	RHI::Buffer m_OutputBufferGPU;		// Default+UAV : GPU
	RHI::Buffer m_OutputBufferReadback;	// Readback : GPU→CPU
};