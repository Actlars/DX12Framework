// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "CoopVecTestRunner.h"
#include <Engine/RHI/Core/Device/Device.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
//		CoopVecTest.slangをDispatchし、結果を検証する
// -------------------------------------------------------------------------------
bool CoopVecTestRunner::Run(RHI::Device* _pDevice)
{
	if (_pDevice == nullptr)
	{
		return false;
	}

	auto* pDevice = _pDevice->GetDevice();

	// -------------------------------------------------------------------------------
	// RootSignature / PSO読み込み
	// -------------------------------------------------------------------------------
	if (!m_RootSignatureLayout.LoadFromJson(pDevice, L"Assets/Config/Json/RootSignature/CoopVecTest.json"))
	{
		ELOG("CoopVecTestRunner::Run() : RootSignatureLayout load failed");
		return false;
	}

	auto* pPSO = _pDevice->GetPipelineCache()->GetOrCreate(
		pDevice,
		L"Assets/Config/Json/PipelineState/CoopVecTest.json",
		m_RootSignatureLayout.GetRootSignature(),
		D3D12_INPUT_LAYOUT_DESC{ nullptr,0 });

	if (pPSO == nullptr)
	{
		ELOG("CoopVecTestRunner::Run() : PSO creation failed");
		return false;
	}

	// -------------------------------------------------------------------------------
	// 入力データを準備（lhs = 0,1,2,...31 / rhs = 1000,1000,...1000）
	// シェーダー側は各スレッドが同じ32要素を読む単純な検証用データにしている
	// -------------------------------------------------------------------------------
	std::vector<int32_t> lhsData(kElementsPerThread);
	std::vector<int32_t> rhsData(kElementsPerThread);
	for (uint32_t i = 0; i < kElementsPerThread; ++i)
	{
		lhsData[i] = static_cast<int32_t>(i);
		rhsData[i] = 1000;
	}

	RHI::BufferDesc inputDesc;
	inputDesc.SizeInBytes	= sizeof(int32_t) * kElementsPerThread;
	inputDesc.HeapType		= RHI::BufferHeapType::Upload;

	if (!m_InputBuffer1.Init(pDevice, inputDesc, rhsData.data()))
	{
		ELOG("CoopVecTestRunner::Run() : InputBuffer1 init failed");
		return false;
	}
	if (!m_InputBuffer2.Init(pDevice, inputDesc, lhsData.data()))
	{
		ELOG("CoopVecTestRunner::Run() : InputBuffer2 init failed");
		return false;
	}

	// -------------------------------------------------------------------------------
	// 出力バッファ（Default + UAV）とReadback用バッファを準備
	// -------------------------------------------------------------------------------
	RHI::BufferDesc outputDesc;
	outputDesc.SizeInBytes = sizeof(int32_t) * kTotalElements;
	outputDesc.HeapType = RHI::BufferHeapType::Default;
	outputDesc.AllowUAV = true;

	if (!m_OutputBufferGPU.Init(pDevice, outputDesc))
	{
		ELOG("CoopVecTestRunner::Run() : OutputBufferGPU init failed");
		return false;
	}

	RHI::BufferDesc readbackDesc;
	readbackDesc.SizeInBytes = sizeof(int32_t) * kTotalElements;
	readbackDesc.HeapType = RHI::BufferHeapType::Readback;

	if (!m_OutputBufferReadback.Init(pDevice, readbackDesc))
	{
		ELOG("CoopVecTestRunner::Run() : OutputBufferReadback init failed");
		return false;
	}

	// -------------------------------------------------------------------------------
	// コマンド記録
	// -------------------------------------------------------------------------------
	auto* pCmdList = _pDevice->GetCommandList()->Reset(_pDevice->GetFence());
	if (pCmdList == nullptr)
	{
		ELOG("CoopVecTestRunner::Run() : CommandList Reset failed");
		return false;
	}

	pCmdList->SetComputeRootSignature(m_RootSignatureLayout.GetRootSignature());
	pCmdList->SetPipelineState(pPSO);

	pCmdList->SetComputeRootShaderResourceView(
		m_RootSignatureLayout.GetSlot("InputBuffer1"), m_InputBuffer1.GetAddress());
	pCmdList->SetComputeRootShaderResourceView(
		m_RootSignatureLayout.GetSlot("InputBuffer2"), m_InputBuffer2.GetAddress());
	pCmdList->SetComputeRootUnorderedAccessView(
		m_RootSignatureLayout.GetSlot("OutputBuffer"), m_OutputBufferGPU.GetAddress());

	// [numthreads(4,4,1)]のシェーダーに対して、スレッドグループ1つだけ起動する
	// （グループ内の 4 * 4 * 1 = 16スレッドがそれぞれ32要素のCoopVec加算を行う）
	pCmdList->Dispatch(1, 1, 1);

	// -------------------------------------------------------------------------------
	// UAV書き込み完了を保証するバリア + Readbackへコピー
	// -------------------------------------------------------------------------------
	{
		// UAV Barrier : 直前のUAV書き込みが、後続の読み取り（コピー）から
		// 確実に見えるようにするための同期バリア
		D3D12_RESOURCE_BARRIER uavBarrier = {};
		uavBarrier.Type				= D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource	= m_OutputBufferGPU.GetResource();
		pCmdList->ResourceBarrier(1, &uavBarrier);

		// UNORDERED_ACCESS → COPU_SORCE は状態遷移
		D3D12_RESOURCE_BARRIER transition = {};
		transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		transition.Transition.pResource = m_OutputBufferGPU.GetResource();
		transition.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		pCmdList->ResourceBarrier(1, &transition);
	}

	pCmdList->CopyBufferRegion(
		m_OutputBufferReadback.GetResource(), 0,
		m_OutputBufferGPU.GetResource(), 0,
		sizeof(int32_t)* kTotalElements);

	pCmdList->Close();

	// -------------------------------------------------------------------------------
	// コマンド実行 → 完了待ち
	// -------------------------------------------------------------------------------
	ID3D12CommandList* pLists[] = { pCmdList };
	_pDevice->GetQueue()->ExecuteCommandLists(1, pLists);

	// このコマンドが使ったアロケータの完了フェンス値を記録してからGPU完了を待つ
	const auto fenceValue = _pDevice->GetFence()->Signal(_pDevice->GetQueue());
	_pDevice->GetCommandList()->RecordFenceValue(fenceValue);
	_pDevice->WaitForGPU();

	// -------------------------------------------------------------------------------
	// 結果検証
	// -------------------------------------------------------------------------------
	std::vector<int32_t> result(kTotalElements);
	m_OutputBufferReadback.Read(result.data(), sizeof(int32_t) * kTotalElements);

	bool allMatch = true;
	for (uint32_t thread = 0; thread < kThreadCount; ++thread)
	{
		for (uint32_t i = 0; i < kElementsPerThread; ++i)
		{
			const int32_t expected = lhsData[i] + rhsData[i];
			const int32_t actual = result[thread * kElementsPerThread + i];
			if (expected != actual)
			{
				ELOG("CoopVecTestRunner::Run() : mismatch at thread = %u i = %u expected = %d actual = %d",
					thread, i, expected, actual);
				allMatch = false;
			}
		}
	}

	if (allMatch)
	{
		ELOG("CoopVecTestRunner::Run() : SUCCESS! CoopVec and result matches on all %u threads", kThreadCount);
	}

	return allMatch;
}
