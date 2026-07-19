#pragma once
#include <Engine/RHI/Pipeline/RootSignature/RootSignatureLayout/RootSignatureLayout.h>
#include <Engine/RHI/Resource/Buffer/Buffer.h>
#include <Engine/RHI/Resource/Texture/Texture.h>

namespace RHI { class Device; }

class NTCImageDecodeTestRunner
{
public:
    static constexpr uint32_t kImageW = 128;
    static constexpr uint32_t kImageH = 64;

    static constexpr uint32_t kGridHighW = 64;
    static constexpr uint32_t kGridHighH = 32;
    static constexpr uint32_t kGridLowW = 16;
    static constexpr uint32_t kGridLowH = 8;

    static constexpr uint32_t kLatentDimHigh = 4;
    static constexpr uint32_t kLatentDimLow = 8;
    static constexpr uint32_t kCombinedDim = kLatentDimHigh + kLatentDimLow;

    static constexpr uint32_t kHiddenDim = 32;
    static constexpr uint32_t kOutputDim = 3;

    bool Run(RHI::Device* _pDevice);

    RHI::Texture& GetBakedTexture() { return m_OutputTexture; }

private:
    std::vector<float> LoadBinaryFloats(const std::wstring& _path);

    RHI::RootSignatureLayout m_RootSignatureLayout;

    RHI::Buffer  m_LatentGridHighBuffer; // Upload
    RHI::Buffer  m_LatentGridLowBuffer;  // Upload
    RHI::Buffer  m_WeightBuffer;         // Upload
    RHI::Texture m_OutputTexture;        // UAV+SRV両対応、ベイク先
};
