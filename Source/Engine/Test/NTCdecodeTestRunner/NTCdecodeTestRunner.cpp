// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "NTCdecodeTestRunner.h"
#include <Engine/RHI/Core/Device/Device.h>
#include <Engine/RHI/Core/CommandList/CommandList.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

bool NTCDecodeTestRunner::Run(RHI::Device* _pDevice)
{
	if (_pDevice == nullptr) 
	{ return false; }

	auto* pDevice = _pDevice->GetDevice();

	// -------------------------------------------------------------------------------
	// RootSignature / PSO読み込み
	// -------------------------------------------------------------------------------
	if (!m_RootSignatureLayout.LoadFromJson(pDevice, L"Assets/Config/Json/RootSignature/NTCDecodeTest.json"))
	{
		ELOG("NTCDecodeTestRunner::Run() : RootSignatureLayout load failed");
		return false;
	}

	auto* pPSO = _pDevice->GetPipelineCache()->GetOrCreate(
		pDevice, L"Assets/Config/Json/PipelineState/NTCDecodeTest.json",
		m_RootSignatureLayout.GetRootSignature(),
		D3D12_INPUT_LAYOUT_DESC{ nullptr,0 });

	if (pPSO == nullptr)
	{
		ELOG("NTCDecodeTestRunner::Run() : PipelineState creation failed");
		return false;
	}

	// -------------------------------------------------------------------------------
	// 検証用データを準備
	// 入力ベクトル : [1,2,3,4]
	// 
	// Layer0 重み(4x4, 行優先) : 単位行列
	// [1,0,0,0]
	// [0,1,0,0]
	// [0,0,1,0]
	// [0,0,0,1]
	// Layer0 バイアス : [0,0,0,0]
	// → 恒等変換 + RaLUなので隠れ層は入力そのまま[1,2,3,4]
	// 
	// Layer1 重み(3x4, 行優先) : 合計パターン
	// [1,1,1,1] → out[0] = 全要素の合計
	// [0,1,0,1] → out[1] = in[1]+in[3]
	// [1,0,1,0] → out[2] = in[0]+in[2]
	// Layer1 バイアス : [0,0,0]
	// → 期待される最終出力は[10,6,4]
	// -------------------------------------------------------------------------------
	std::vector<float> vecData = { 1.0f,2.0f,3.0f,4.0f };
	std::vector<float> weightData = {
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		0,0,0,1,
		0,0,0,0,
		1,1,1,1,
		0,1,0,1,
		1,0,1,0,
		0,0,0 };

	// 入力ベクトルの作成
	RHI::BufferDesc vecDesc;
	vecDesc.SizeInBytes = sizeof(float) * kInputDim;
	vecDesc.HeapType	= RHI::BufferHeapType::Upload;

	if (!m_InputVector.Init(pDevice, vecDesc, vecData.data()))
	{
		ELOG("NTCDecodeTestRunner::Run : InputVector init failed");
		return false;
	}

	// 重みバッファの作成（Layer0 + Layer1を連結）
	RHI::BufferDesc weightDesc;
	weightDesc.SizeInBytes	= sizeof(float) * static_cast<int32_t>(weightData.size());
	weightDesc.HeapType		= RHI::BufferHeapType::Upload;

	if (!m_WeightBuffer.Init(pDevice, weightDesc, weightData.data()))
	{
		ELOG("NTCDecodeTestRunner::Run : WeightBuffer init failed");
		return false;
	}
	RHI::BufferDesc outDesc;
	outDesc.SizeInBytes = sizeof(float) * kOutputDim;
	outDesc.HeapType	= RHI::BufferHeapType::Default;
	outDesc.AllowUAV	= true;

	if (!m_OutputBufferGPU.Init(pDevice, outDesc))
	{
		ELOG("NTCDecodeTestRunner::Run : OutputBufferGPU init failed");
		return false;
	}

	RHI::BufferDesc readbackDesc;
	readbackDesc.SizeInBytes	= sizeof(float) * kOutputDim;
	readbackDesc.HeapType		= RHI::BufferHeapType::Readback;

	if (!m_OutputBufferReadback.Init(pDevice, readbackDesc))
	{
		ELOG("NTCDecodeTestRunner::Run : OutputBufferReadback init failed");
		return false;
	}

	// -------------------------------------------------------------------------------
	// コマンド記録
	// -------------------------------------------------------------------------------
	auto* pCmdList = _pDevice->GetCommandList()->Reset(_pDevice->GetFence());
	if (pCmdList == nullptr)
	{
		ELOG("NTCDecodeTestRunner::Run() : CommandList Reset failed");
		return false;
	}

	pCmdList->SetComputeRootSignature(m_RootSignatureLayout.GetRootSignature());
	pCmdList->SetPipelineState(pPSO);

	pCmdList->SetComputeRootShaderResourceView(
		m_RootSignatureLayout.GetSlot("InputVector"), m_InputVector.GetAddress());
	pCmdList->SetComputeRootShaderResourceView(
		m_RootSignatureLayout.GetSlot("WeightBuffer"), m_WeightBuffer.GetAddress());
	pCmdList->SetComputeRootUnorderedAccessView(
		m_RootSignatureLayout.GetSlot("OutputBuffer"), m_OutputBufferGPU.GetAddress());

	{
		D3D12_RESOURCE_BARRIER toUAV = {};
		toUAV.Type						= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		toUAV.Transition.pResource		= m_OutputBufferGPU.GetResource();
		toUAV.Transition.StateBefore	= D3D12_RESOURCE_STATE_COMMON;
		toUAV.Transition.StateAfter		= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		toUAV.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		pCmdList->ResourceBarrier(1, &toUAV);
	}

	pCmdList->Dispatch(1, 1, 1);	// 1スレッドだけで十分

	{
		D3D12_RESOURCE_BARRIER uavBarrier = {};
		uavBarrier.Type				= D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource	= m_OutputBufferGPU.GetResource();
		pCmdList->ResourceBarrier(1, &uavBarrier);

		D3D12_RESOURCE_BARRIER transition = {};
		transition.Type						= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		transition.Transition.pResource		= m_OutputBufferGPU.GetResource();
		transition.Transition.StateBefore	= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		transition.Transition.StateAfter	= D3D12_RESOURCE_STATE_COPY_SOURCE;
		transition.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		pCmdList->ResourceBarrier(1, &transition);
	}

	pCmdList->CopyBufferRegion(
		m_OutputBufferReadback.GetResource(), 0,
		m_OutputBufferGPU.GetResource(), 0,
		sizeof(float) * kOutputDim);

	pCmdList->Close();

	ID3D12CommandList* pLists[] = { pCmdList };
	_pDevice->GetQueue()->ExecuteCommandLists(1, pLists);

	const auto fenceValue = _pDevice->GetFence()->Signal(_pDevice->GetQueue());
	_pDevice->GetCommandList()->RecordFenceValue(fenceValue);
	_pDevice->WaitForGPU();

	// -------------------------------------------------------------------------------
	// 結果検証: CPU側でも同じ行列×ベクトル計算を行い、一致するか確認する
	// -------------------------------------------------------------------------------
	std::vector<float> result(kOutputDim);
	m_OutputBufferReadback.Read(result.data(), sizeof(float) * kOutputDim);

	// CPU側でLayer0 (4x4 unit matrix + ReLU) を計算
	std::vector<float> hidden(kHiddenDim);
	for (uint32_t o = 0; o < kHiddenDim; ++o)
	{
		float sum = weightData[kInputDim * kHiddenDim + o]; // Layer0 bias
		for (uint32_t i = 0; i < kInputDim; ++i)
		{
			sum += vecData[i] * weightData[o * kInputDim + i];
		}
		hidden[o] = (sum > 0.0f) ? sum : 0.0f; // ReLU
	}

	// CPU側でLayer1 (3x4 合計パターン, 活性化なし) を計算
	const uint32_t layer1WeightOffset = kInputDim * kHiddenDim + kHiddenDim;
	const uint32_t layer1BiasOffset = layer1WeightOffset + kHiddenDim * kOutputDim;

	std::vector<float> expected(kOutputDim);
	for (uint32_t o = 0; o < kOutputDim; ++o)
	{
		float sum = weightData[layer1BiasOffset + o]; // Layer1 bias
		for (uint32_t i = 0; i < kHiddenDim; ++i)
		{
			sum += hidden[i] * weightData[layer1WeightOffset + o * kHiddenDim + i];
		}
		expected[o] = sum;
	}

	bool allMatch = true;
	for (uint32_t o = 0; o < kOutputDim; ++o)
	{
		if (expected[o] != result[o])
		{
			ELOG("NTCDecodeTestRunner::Run() : mismatch at index = %u expected = %f actual = %f",
				o, expected[o], result[o]);
			allMatch = false;
		}
	}

	if (allMatch)
	{
		ELOG("NTCDecodeTestRunner::Run() : SUCCESS! software FMA MLP result matches (expected [10, 6, 4])");
	}

	return allMatch;
}
