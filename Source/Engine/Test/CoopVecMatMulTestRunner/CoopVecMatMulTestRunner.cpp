#include "CoopVecMatMulTestRunner.h"
#include <Engine/RHI/Core/Device/Device.h>
#include <Engine/RHI/Core/CommandList/CommandList.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

bool CoopVecMatMulTestRunner::Run(RHI::Device* _pDevice)
{
	if (_pDevice == nullptr) 
	{ return false; }

	auto* pDevice = _pDevice->GetDevice();

	// -------------------------------------------------------------------------------
	// RootSignature / PSO読み込み
	// -------------------------------------------------------------------------------
	if (!m_RootSignatureLayout.LoadFromJson(pDevice, L"Assets/Config/Json/RootSignature/CoopVecMatMulTest.json"))
	{
		ELOG("CoopVecMatMulTestRunner::Run() : RootSignatureLayout load failed");
		return false;
	}

	auto* pPSO = _pDevice->GetPipelineCache()->GetOrCreate(
		pDevice, L"Assets/COnfig/Json/PipelineState/CoopVecMatMulTest.json",
		m_RootSignatureLayout.GetRootSignature(),
		D3D12_INPUT_LAYOUT_DESC{ nullptr,0 });

	if (pPSO == nullptr)
	{
		ELOG("CoopVecmatMulTestRunner::Run() : PipelineState creation failed");
		return false;
	}

	// -------------------------------------------------------------------------------
	// 検証用データを準備
	// 入力ベクトル : [1,2,3,4]
	// 重み行列(4*4, 行優先) : 
	//	[1,0,0,0]
	//	[0,1,0,0]
	//	[0,0,1,0]
	//	[0,0,0,1]
	//　→単位行列なので、期待される結果は入力ベクトルそのまま[1,2,3,4]
	//	（まずは「行列をかけても値が変わらない」という一番検証しやすいケースから確認）
	// -------------------------------------------------------------------------------
	std::vector<int32_t> vecData = { 1,2,3,4 };
	std::vector<int32_t> matData = {
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		0,0,0,1 };

	// 入力ベクトルの作成
	RHI::BufferDesc vecDesc;
	vecDesc.SizeInBytes = sizeof(int32_t) * kDim;
	vecDesc.HeapType	= RHI::BufferHeapType::Upload;

	if (m_InputVector.Init(pDevice, vecDesc, vecData.data()))
	{
		ELOG("CoopVecMatMulRunner::Run : InputVector init failed");
		return false;
	}

	// 重み行列の作成
	RHI::BufferDesc matDesc;
	matDesc.SizeInBytes = sizeof(int32_t) * kDim * kDim;
	matDesc.HeapType	= RHI::BufferHeapType::Upload;

	if (m_WeightMatrix.Init(pDevice, matDesc, matData.data()))
	{
		ELOG("CoopVecMatMulRunner::Run : WeightMatrix init failed");
		return false;
	}
	RHI::BufferDesc outDesc;
	outDesc.SizeInBytes = sizeof(int32_t) * kDim;
	outDesc.HeapType	= RHI::BufferHeapType::Default;
	outDesc.AllowUAV	= true;

	if (m_OutputBufferGPU.Init(pDevice, outDesc))
	{
		ELOG("CoopVecMatMulRunner::Run : OutputBufferGPU init failed");
		return false;
	}

	RHI::BufferDesc readbackDesc;
	readbackDesc.SizeInBytes	= sizeof(int32_t) * kDim;
	readbackDesc.HeapType		= RHI::BufferHeapType::Readback;

	if (m_OutputBufferReadback.Init(pDevice, readbackDesc))
	{
		ELOG("CoopVecMatMulRunner::Run : OutputBufferReadback init failed");
		return false;
	}

	// -------------------------------------------------------------------------------
	// コマンド記録
	// -------------------------------------------------------------------------------
	auto* pCmdList = _pDevice->GetCommandList()->Reset(_pDevice->GetFence());
	if (pCmdList == nullptr)
	{
		ELOG("CoopVecMatMulTestRunner::Run() : CommandList Reset failed");
		return false;
	}

	pCmdList->SetComputeRootSignature(m_RootSignatureLayout.GetRootSignature());
	pCmdList->SetPipelineState(pPSO);

	pCmdList->SetComputeRootShaderResourceView(
		m_RootSignatureLayout.GetSlot("InputVector"), m_InputVector.GetAddress());
	pCmdList->SetComputeRootShaderResourceView(
		m_RootSignatureLayout.GetSlot("WeightMatrix"), m_WeightMatrix.GetAddress());
	pCmdList->SetComputeRootUnorderedAccessView(
		m_RootSignatureLayout.GetSlot("OutputBuffer"), m_OutputBufferGPU.GetAddress());

	{
		D3D12_RESOURCE_BARRIER toUAV = {};
		toUAV.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		toUAV.Transition.pResource = m_OutputBufferGPU.GetResource();
		toUAV.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		toUAV.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		toUAV.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		pCmdList->ResourceBarrier(1, &toUAV);
	}

	pCmdList->Dispatch(1, 1, 1);	// 1スレッドだけで十分

	{
		D3D12_RESOURCE_BARRIER uavBarrier = {};
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = m_OutputBufferGPU.GetResource();
		pCmdList->ResourceBarrier(1, &uavBarrier);

		D3D12_RESOURCE_BARRIER transition = {};
		transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		transition.Transition.pResource = m_OutputBufferGPU.GetResource();
		transition.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		pCmdList->ResourceBarrier(1, &transition);
	}

	pCmdList->CopyBufferRegion(
		m_OutputBufferGPU.GetResource(), 0,
		m_OutputBufferReadback.GetResource(), 0,
		sizeof(int32_t) * kDim);

	pCmdList->Close();

	ID3D12CommandList* pLists[] = { pCmdList };
	_pDevice->GetQueue()->ExecuteCommandLists(1, pLists);

	const auto fenceValue = _pDevice->GetFence()->Signal(_pDevice->GetQueue());
	_pDevice->GetCommandList()->RecordFenceValue(fenceValue);
	_pDevice->WaitForGPU();

	// -------------------------------------------------------------------------------
	// 結果検証: CPU側でも同じ行列×ベクトル計算を行い、一致するか確認する
	// -------------------------------------------------------------------------------
	std::vector<int32_t> result(kDim);
	m_OutputBufferReadback.Read(result.data(), sizeof(int32_t) * kDim);

	bool allMatch = true;
	for (uint32_t row = 0; row < kDim; ++row)
	{
		int32_t expected = 0;
		for (uint32_t col = 0; col < kDim; ++col)
		{
			expected += matData[row * kDim + col] * vecData[col];
		}

		if (expected != result[row])
		{
			ELOG("CoopVecMatMulTestRunner::Run() : mismatch at row = %u expected = %d actual = %d",
				row, expected, result[row]);
			allMatch = false;
		}
	}

	if (allMatch)
	{
		ELOG("CoopVecMatMulTestRunner::Run() : SUCCESS! coopVecMatMul result matches (identity matrix)");
	}

	return allMatch;
}
