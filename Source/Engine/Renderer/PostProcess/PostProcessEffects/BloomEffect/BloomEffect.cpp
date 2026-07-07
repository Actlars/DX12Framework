// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "BloomEffect.h"
#include <Engine/RHI/Core/Device/Device.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

bool BloomEffect::Init(RHI::Device* _pDevice)
{
	m_pDevice = _pDevice;
	auto* pDevice = _pDevice->GetDevice();

	if (!m_RootSignatureLayout.LoadFromJson(pDevice, L"Assets/Config/Json/RootSignature/PostProcess.json"))
	{
		ELOG("BloomEffect::Init() RootSignature load failed");
		return false;
	}

	auto* pPostProcessRS = m_RootSignatureLayout.GetRootSignature();
	m_pExtractPSO = m_pDevice->GetPipelineCache()->GetOrCreate(
		m_pDevice->GetDevice(), L"Assets/Config/Json/PipelineState/BloomExtract.json",
		pPostProcessRS, D3D12_INPUT_LAYOUT_DESC{ nullptr,0 });  // 頂点レイアウトなしなのでレイアウトは空
	m_pBlurPSO = m_pDevice->GetPipelineCache()->GetOrCreate(
		m_pDevice->GetDevice(), L"Assets/Config/Json/PipelineState/BloomBlur.json",
		pPostProcessRS, D3D12_INPUT_LAYOUT_DESC{ nullptr,0 });
	m_pCompositePSO = m_pDevice->GetPipelineCache()->GetOrCreate(
		m_pDevice->GetDevice(), L"Assets/Config/Json/PipelineState/BloomComposite.json",
		pPostProcessRS, D3D12_INPUT_LAYOUT_DESC{ nullptr,0 });

	if (m_pExtractPSO == nullptr || m_pBlurPSO == nullptr || m_pCompositePSO == nullptr) 
	{ 
		ELOG("BloomEffect::Init() PSO creation failed");
		return false;
	}

	m_BlurParams = std::make_unique<RHI::ConstantBuffer>();
	m_BlurParams->Init(pDevice, m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES), sizeof(BlurParamsCB));
	auto* pU = m_BlurParams->GetPtr<BlurParamsCB>();
	pU->TexelSize = { 1.0f / (m_pDevice->GetWidth() / 2.0f) , 1.0f / (m_pDevice->GetHeight() / 2.0f) };

	return true;
}

void BloomEffect::AddPasses(RG::RenderGraph& _graph, RG::Handle& _sceneColorHandle, RG::Handle _backBufferHandle, bool _isLast)
{
    // Transientバッファを確保（画面の半分サイズ、ブルーム用）
    RG::TransientResourceDesc bloomDesc;
    bloomDesc.Width         = m_pDevice->GetWidth() / 2;
    bloomDesc.Height        = m_pDevice->GetHeight() / 2;
    bloomDesc.Format        = DXGI_FORMAT_R16G16B16A16_FLOAT;
    bloomDesc.ClearColor[0] = bloomDesc.ClearColor[1] = bloomDesc.ClearColor[2] = 0.0f;
    bloomDesc.ClearColor[3] = 1.0f;

    // A: Extractの出力先、Blurの入力元として使う
    auto bloomHandle = _graph.GetRegistry().CreateTransient(
        m_pDevice->GetDevice(), "BloomBuffer", bloomDesc,
        m_pDevice->GetTransientResourcePool(),
        m_pDevice->GetPool(RHI::Device::POOL_TYPE_RTV),
        m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES));    // RTV + SRV 両方発行

    // B: Blurの出力先、Compositeの入力元として使う
    auto bloomHandleB = _graph.GetRegistry().CreateTransient(
        m_pDevice->GetDevice(), "BloomBufferB", bloomDesc,
        m_pDevice->GetTransientResourcePool(),
        m_pDevice->GetPool(RHI::Device::POOL_TYPE_RTV),
        m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES));

    // -------------------------------------------------------------------------------
    // Extractパス : sceneColorHandleを読み、明るい部分だけをBloomHandle（A）に書き込む
    // -------------------------------------------------------------------------------
    _graph.AddPass("BloomExtract", [bloomHandle, _sceneColorHandle](RG::PassBuilder& b)
        {
            // Setup : このパスが使うリソースと要求ステートを宣言する
            b.Use(_sceneColorHandle,    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            b.Use(bloomHandle,          D3D12_RESOURCE_STATE_RENDER_TARGET);
        },
        // Execute : Setupの宣言に基づき、RenderGraphがバリア解決済みの状態で呼ばれる
        [this, _sceneColorHandle, bloomHandle](ID3D12GraphicsCommandList* cmd, const RG::ResourceRegistry& res)
        {
            // 描画先をbloomHandle(A)に切り替える
            // DX12はOMSetRenderTargetsを呼びなおさない限り前のパスの設定が残り続けるため、
            // パスが変わるたびに毎回明示する
            auto* pRTV = res.GetRTV(bloomHandle);
            cmd->OMSetRenderTargets(1, &pRTV->HandleCPU, FALSE, nullptr);

            // bloomHandleは画面の半分のサイズで確保する
            // フルサイズのビューポートだと、描画範囲がずれるため、TexelSizeに合わせておく
            SetFullViewport(cmd, m_pDevice->GetWidth() / 2, m_pDevice->GetHeight() / 2);

            cmd->SetGraphicsRootSignature(m_RootSignatureLayout.GetRootSignature());
            cmd->SetPipelineState(m_pExtractPSO);

            ID3D12DescriptorHeap* heaps[] = { m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES)->GetHeap() };
            cmd->SetDescriptorHeaps(_countof(heaps), heaps);

            // MainPassが描いた絵（sceneColorHandle）をSRVとしてシェーダーに渡す
            auto* pSrcSRV = res.GetSRV(_sceneColorHandle);
            cmd->SetGraphicsRootDescriptorTable(
                m_RootSignatureLayout.GetSlot("SourceTexture"), pSrcSRV->HandleGPU);

            // 頂点バッファ・インデックスバッファなしで、頂点IDだけから
            // 画面全体を覆う三角形を1枚生成する
            cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cmd->DrawInstanced(3, 1, 0, 0);
        });

    // -------------------------------------------------------------------------------
    // Blurパス : bloomHandle(A)に5x5ガウシアンブラーをかけて、bloomHandleB(B)に書く
    // -------------------------------------------------------------------------------
    _graph.AddPass("BloomBlur", [bloomHandle, bloomHandleB](RG::PassBuilder& b)
        {
            b.Use(bloomHandle,  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); // A : 読む
            b.Use(bloomHandleB, D3D12_RESOURCE_STATE_RENDER_TARGET);        // B : 書く
        },
        [this, bloomHandle, bloomHandleB](ID3D12GraphicsCommandList* cmd, const RG::ResourceRegistry& res)
        {
            auto* pRTV = res.GetRTV(bloomHandleB);
            cmd->OMSetRenderTargets(1, &pRTV->HandleCPU, FALSE, nullptr);

            // bloomHandleBもbloomHandleと同じ半分サイズなので、同じビューポートを使う
            SetFullViewport(cmd, m_pDevice->GetWidth() / 2, m_pDevice->GetHeight() / 2);

            cmd->SetGraphicsRootSignature(m_RootSignatureLayout.GetRootSignature());
            cmd->SetPipelineState(m_pBlurPSO);

            ID3D12DescriptorHeap* heaps[] = { m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES)->GetHeap() };
            cmd->SetDescriptorHeaps(_countof(heaps), heaps);

            // ぼかす対象はExtractの結果(A)BlurPS.hlsl側で
            // 5x5ガウシアンカーネルを（重みの合計 = 1.0になるように正規化済み）適用する
            auto* pSrcSRV = res.GetSRV(bloomHandle);
            cmd->SetGraphicsRootDescriptorTable(
                m_RootSignatureLayout.GetSlot("SourceTexture"), pSrcSRV->HandleGPU);

            // TexelSize(1 / バッファ幅、1 / バッファ高さ)をシェーダーに渡す
            // これがないと、シェーダー内でサンプリング位置を何ピクセルずらせばいいのかわからない
            cmd->SetGraphicsRootConstantBufferView(
                m_RootSignatureLayout.GetSlot("BlurParams"), m_BlurParams->GetAddress());

            cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cmd->DrawInstanced(3, 1, 0, 0);
        });

    // -------------------------------------------------------------------------------
    // Compositeパス : sceneColorHandle(元の絵) + bloomHandleB(ぼかし済みブルーム)を
    //                 加算合成し、colorHandle(バックバッファ)に書き込む
    // -------------------------------------------------------------------------------
    _graph.AddPass("BloomComposite", [_sceneColorHandle, bloomHandleB, _backBufferHandle](RG::PassBuilder& b)
        {
            b.Use(_sceneColorHandle,    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);    // 元の絵を読む
            b.Use(bloomHandleB,         D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);    // ぼかし済みを読む
            b.Use(_backBufferHandle,    D3D12_RESOURCE_STATE_RENDER_TARGET);            // バックバッファに書く
        },
        [this, _sceneColorHandle, bloomHandleB, _backBufferHandle](ID3D12GraphicsCommandList* cmd, const RG::ResourceRegistry& res)
        {
            // colorHandleはImportリソース（バックバッファ）なので、Transientのように
            // ResourceRegistry::GetRTV()では取得できない。GraphicsDevice側から直接取得する
            const auto frameIndex = m_pDevice->GetFrameIndex();
            auto handleRTV = m_pDevice->GetColorTarget(frameIndex)->GetHandleRTV()->HandleCPU;
            cmd->OMSetRenderTargets(1, &handleRTV, FALSE, nullptr);

            // バックバッファはフルサイズなので、直前のBlurパス（半分）から
            // ビューポートをフルサイズに戻す
            SetFullViewport(cmd, m_pDevice->GetWidth(), m_pDevice->GetHeight());

            cmd->SetGraphicsRootSignature(m_RootSignatureLayout.GetRootSignature());
            cmd->SetPipelineState(m_pCompositePSO);

            ID3D12DescriptorHeap* heaps[] = { m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES)->GetHeap() };
            cmd->SetDescriptorHeaps(_countof(heaps), heaps);

            // t0 : 元のシーンの絵
            auto* pSrcSRV = res.GetSRV(_sceneColorHandle);
            cmd->SetGraphicsRootDescriptorTable(
                m_RootSignatureLayout.GetSlot("SourceTexture"), pSrcSRV->HandleGPU);

            // t1 : ぼかし済みのブルーム結果（Blurパスの出力 = B）
            auto* pBloomSRV = res.GetSRV(bloomHandleB);
            cmd->SetGraphicsRootDescriptorTable(
                m_RootSignatureLayout.GetSlot("BloomTexture"), pBloomSRV->HandleGPU);

            cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cmd->DrawInstanced(3, 1, 0, 0);
        });
}

void BloomEffect::SetFullViewport(ID3D12GraphicsCommandList* _pCmd, uint32_t _width, uint32_t _height)
{
    D3D12_VIEWPORT  vp      = { 0.0f,0.0f,static_cast<float>(_width), static_cast<float>(_height) };
    D3D12_RECT      scissor = { 0,0,static_cast<LONG>(_width), static_cast<LONG>(_height) };
    _pCmd->RSSetViewports(1, &vp);
    _pCmd->RSSetScissorRects(1, &scissor);
}
