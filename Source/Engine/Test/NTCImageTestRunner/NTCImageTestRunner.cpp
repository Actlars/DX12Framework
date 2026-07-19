#include "NTCImageTestRunner.h"
#include <Engine/RHI/Core/Device/Device.h>
#include <Engine/RHI/Core/CommandList/CommandList.h>
#include <Engine/Utility/Debug/Logger/Logger.h>
#include <fstream>

bool NTCImageDecodeTestRunner::Run(RHI::Device* _pDevice)
{
    if (_pDevice == nullptr)
    {
        return false;
    }

    auto* pDevice = _pDevice->GetDevice();

    if (!m_RootSignatureLayout.LoadFromJson(pDevice, L"Assets/Config/Json/RootSignature/NTCImageDecodeTest.json"))
    {
        ELOG("NTCImageDecodeTestRunner::Run() : RootSignatureLayout load failed");
        return false;
    }

    auto* pPSO = _pDevice->GetPipelineCache()->GetOrCreate(
        pDevice, L"Assets/Config/Json/PipelineState/NTCImageDecodeTest.json",
        m_RootSignatureLayout.GetRootSignature(),
        D3D12_INPUT_LAYOUT_DESC{ nullptr,0 });

    if (pPSO == nullptr)
    {
        ELOG("NTCImageDecodeTestRunner::Run() : PipelineState creation failed");
        return false;
    }

    // -------------------------------------------------------------------------------
    // Python学習済みデータの読み込み(2階層グリッド分)
    // -------------------------------------------------------------------------------
    std::vector<float> latentHighData = LoadBinaryFloats(L"Assets/NTC/Test/latent_grid_high.bin");
    std::vector<float> latentLowData = LoadBinaryFloats(L"Assets/NTC/Test/latent_grid_low.bin");
    std::vector<float> weightData = LoadBinaryFloats(L"Assets/NTC/Test/weights.bin");

    const size_t expectedHighCount = size_t(kGridHighW) * kGridHighH * kLatentDimHigh;
    const size_t expectedLowCount = size_t(kGridLowW) * kGridLowH * kLatentDimLow;
    const size_t expectedWeightCount =
        size_t(kCombinedDim) * kHiddenDim + kHiddenDim +
        size_t(kHiddenDim) * kOutputDim + kOutputDim;

    if (latentHighData.size() != expectedHighCount)
    {
        ELOG("NTCImageDecodeTestRunner::Run() : latent_grid_high.bin size mismatch expected=%zu actual=%zu",
            expectedHighCount, latentHighData.size());
        return false;
    }
    if (latentLowData.size() != expectedLowCount)
    {
        ELOG("NTCImageDecodeTestRunner::Run() : latent_grid_low.bin size mismatch expected=%zu actual=%zu",
            expectedLowCount, latentLowData.size());
        return false;
    }
    if (weightData.size() != expectedWeightCount)
    {
        ELOG("NTCImageDecodeTestRunner::Run() : weights.bin size mismatch expected=%zu actual=%zu",
            expectedWeightCount, weightData.size());
        return false;
    }

    // -------------------------------------------------------------------------------
    // バッファ作成
    // -------------------------------------------------------------------------------
    RHI::BufferDesc highDesc;
    highDesc.SizeInBytes = sizeof(float) * static_cast<uint32_t>(latentHighData.size());
    highDesc.HeapType = RHI::BufferHeapType::Upload;
    if (!m_LatentGridHighBuffer.Init(pDevice, highDesc, latentHighData.data()))
    {
        ELOG("NTCImageDecodeTestRunner::Run() : LatentGridHighBuffer init failed");
        return false;
    }

    RHI::BufferDesc lowDesc;
    lowDesc.SizeInBytes = sizeof(float) * static_cast<uint32_t>(latentLowData.size());
    lowDesc.HeapType = RHI::BufferHeapType::Upload;
    if (!m_LatentGridLowBuffer.Init(pDevice, lowDesc, latentLowData.data()))
    {
        ELOG("NTCImageDecodeTestRunner::Run() : LatentGridLowBuffer init failed");
        return false;
    }

    RHI::BufferDesc weightDesc;
    weightDesc.SizeInBytes = sizeof(float) * static_cast<uint32_t>(weightData.size());
    weightDesc.HeapType = RHI::BufferHeapType::Upload;
    if (!m_WeightBuffer.Init(pDevice, weightDesc, weightData.data()))
    {
        ELOG("NTCImageDecodeTestRunner::Run() : WeightBuffer init failed");
        return false;
    }

    // -------------------------------------------------------------------------------
    // ベイク先テクスチャの作成
    // -------------------------------------------------------------------------------
    auto* pDescriptorPool = _pDevice->GetPool(RHI::Device::POOL_TYPE_RES);

    if (!m_OutputTexture.InitAsUAVTarget(
        pDevice, pDescriptorPool, kImageW, kImageH, DXGI_FORMAT_R16G16B16A16_FLOAT))
    {
        ELOG("NTCImageDecodeTestRunner::Run() : OutputTexture InitAsUAVTarget failed");
        return false;
    }

    // -------------------------------------------------------------------------------
    // コマンド記録
    // -------------------------------------------------------------------------------
    auto* pCmdList = _pDevice->GetCommandList()->Reset(_pDevice->GetFence());
    if (pCmdList == nullptr)
    {
        ELOG("NTCImageDecodeTestRunner::Run() : CommandList Reset failed");
        return false;
    }

    ID3D12DescriptorHeap* pHeaps[] = { pDescriptorPool->GetHeap() };
    pCmdList->SetDescriptorHeaps(1, pHeaps);

    pCmdList->SetComputeRootSignature(m_RootSignatureLayout.GetRootSignature());
    pCmdList->SetPipelineState(pPSO);

    pCmdList->SetComputeRootShaderResourceView(
        m_RootSignatureLayout.GetSlot("LatentGridHigh"), m_LatentGridHighBuffer.GetAddress());
    pCmdList->SetComputeRootShaderResourceView(
        m_RootSignatureLayout.GetSlot("LatentGridLow"), m_LatentGridLowBuffer.GetAddress());
    pCmdList->SetComputeRootShaderResourceView(
        m_RootSignatureLayout.GetSlot("WeightBuffer"), m_WeightBuffer.GetAddress());

    const uint32_t outputSlot = m_RootSignatureLayout.GetSlot("OutputTexture");
    if (outputSlot == UINT32_MAX)
    {
        ELOG("NTCImageDecodeTestRunner::Run() : OutputTexture slot not found");
        return false;
    }
    pCmdList->SetComputeRootDescriptorTable(outputSlot, m_OutputTexture.GetHandleGPU_UAV());

    const uint32_t groupX = (kImageW + 7) / 8;
    const uint32_t groupY = (kImageH + 7) / 8;
    pCmdList->Dispatch(groupX, groupY, 1);

    {
        D3D12_RESOURCE_BARRIER uavBarrier = {};
        uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier.UAV.pResource = m_OutputTexture.GetResource();
        pCmdList->ResourceBarrier(1, &uavBarrier);

        D3D12_RESOURCE_BARRIER transition = {};
        transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        transition.Transition.pResource = m_OutputTexture.GetResource();
        transition.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        transition.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        pCmdList->ResourceBarrier(1, &transition);
    }

    pCmdList->Close();

    ID3D12CommandList* pLists[] = { pCmdList };
    _pDevice->GetQueue()->ExecuteCommandLists(1, pLists);

    const auto fenceValue = _pDevice->GetFence()->Signal(_pDevice->GetQueue());
    _pDevice->GetCommandList()->RecordFenceValue(fenceValue);
    _pDevice->WaitForGPU();

    ELOG("NTCImageDecodeTestRunner::Run() : SUCCESS! baked NTC texture ready (index=%u)",
        m_OutputTexture.GetIndex());

    return true;
}

std::vector<float> NTCImageDecodeTestRunner::LoadBinaryFloats(const std::wstring& _path)
{
    std::ifstream file(_path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        ELOG("NTCImageDecodeTestRunner::LoadBinaryFloats : failed to open file");
        return {};
    }

    const std::streamsize sizeBytes = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<float> data(static_cast<size_t>(sizeBytes) / sizeof(float));
    if (!file.read(reinterpret_cast<char*>(data.data()), sizeBytes))
    {
        ELOG("NTCImageDecodeTestRunner::LoadBinaryFloats : failed to read file");
        return {};
    }
    return data;
}