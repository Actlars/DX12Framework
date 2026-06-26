// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Application.h"
#include <Framework/Utility/Debug/Logger/Logger.h>
#include <Framework/Mesh/ResData.h>
#include <Framework/Utility/FileUtil/FileUtil.h>
#include <d3dcompiler.h>
#include <d3dx12.h>

#pragma comment(lib, "d3dcompiler.lib")

// -------------------------------------------------------------------------------
// 定数
// -------------------------------------------------------------------------------
namespace
{
    constexpr auto WindowClassName = TEXT("DX12FrameworkWindow");

    // RootParameter スロット番号
    // シェーダーの register と対応させる
    constexpr uint32_t ROOT_PARAM_CBV_TRANSFORM = 0; // b0: 変換行列
    constexpr uint32_t ROOT_PARAM_CBV_MATERIAL = 1; // b1: マテリアルパラメータ
    constexpr uint32_t ROOT_PARAM_SRV_DIFFUSE = 2; // t0: ディフューズテクスチャ

} // namespace

// -------------------------------------------------------------------------------
// コンストラクタ
// -------------------------------------------------------------------------------
Application::Application(uint32_t _width, uint32_t _height)
    : m_Width(_width)
    , m_Height(_height)
{ /* DO_NOTHING */
}

// -------------------------------------------------------------------------------
// デストラクタ
// -------------------------------------------------------------------------------
Application::~Application()
{ /* DO_NOTHING */
}

// -------------------------------------------------------------------------------
// 実行
// -------------------------------------------------------------------------------
void Application::Run()
{
    if (!Init())
    {
        ELOG("Application::Init() failed.");
        return;
    }

    MainLoop();
    Term();
}

// -------------------------------------------------------------------------------
// 初期化
// -------------------------------------------------------------------------------
bool Application::Init()
{
    // ─── ウィンドウ生成 ───
    if (!InitWnd())
    {
        ELOG("Application::InitWnd() failed.");
        return false;
    }

    // ─── GraphicsDevice 初期化 ───
    // DX12 の低レベル処理は全て GraphicsDevice に委譲する
    GraphicsDevice::Desc gfxDesc;
    gfxDesc.hWnd = m_hWnd;
    gfxDesc.Width = m_Width;
    gfxDesc.Height = m_Height;
    gfxDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    gfxDesc.DepthFormat = DXGI_FORMAT_D32_FLOAT;
    gfxDesc.FrameCount = 2;

    if (!m_GraphicsDevice.Init(gfxDesc))
    {
        ELOG("GraphicsDevice::Init() failed.");
        return false;
    }

    // ─── RootSignature 生成 ───
    if (!InitRootSignature())
    {
        ELOG("Application::InitRootSignature() failed.");
        return false;
    }

    // ─── PSO 生成 ───
    if (!InitPipelineState())
    {
        ELOG("Application::InitPipelineState() failed.");
        return false;
    }

    // ─── シーン（メッシュ・マテリアル）ロード ───
    if (!InitScene())
    {
        ELOG("Application::InitScene() failed.");
        return false;
    }

    return true;
}

// -------------------------------------------------------------------------------
// メインループ
// -------------------------------------------------------------------------------
void Application::MainLoop()
{
    MSG msg = {};

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) == TRUE)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Render();
        }
    }
}

// -------------------------------------------------------------------------------
// 終了処理
// -------------------------------------------------------------------------------
void Application::Term()
{
    // Material / Mesh を先に解放（GPU リソースを持つため GraphicsDevice より先）
    m_Materials.clear();
    m_Meshes.clear();

    m_pPSO.Reset();
    m_pRootSignature.Reset();

    // GraphicsDevice は GPU 完了を待ってから解放する（内部で Fence::Sync）
    m_GraphicsDevice.Term();

    TermWnd();
}

// -------------------------------------------------------------------------------
// 描画処理
// -------------------------------------------------------------------------------
void Application::Render()
{
    // ─── 更新処理 ───
    m_RotateAngle += 0.025f;

    // ─── フレーム開始 ───
    // バリア・クリア・RTV 設定・ビューポート設定が内部で行われる
    auto* pCmd = m_GraphicsDevice.BeginFrame();

    // ─── 描画コマンドの組み立て ───
    {
        pCmd->SetGraphicsRootSignature(m_pRootSignature.Get());

        // DescriptorHeap をセット（RES プールのヒープ）
        ID3D12DescriptorHeap* heaps[] =
        {
            m_GraphicsDevice.GetPool(GraphicsDevice::POOL_TYPE_RES)->GetHeap()
        };
        pCmd->SetDescriptorHeaps(1, heaps);

        pCmd->SetPipelineState(m_pPSO.Get());

        // ─── 変換行列（定数バッファ）のセット ───
        // TODO: CameraComponent / TransformComponent を作ったらここに移動する
        // 現状は簡易的にその場で計算する
        struct Transform
        {
            DirectX::XMMATRIX World;
            DirectX::XMMATRIX View;
            DirectX::XMMATRIX Proj;
        };

        // 毎フレーム書き換えるためフレームごとに定数バッファを持つべきだが、
        // 現時点ではシンプルに GPU 仮想アドレスを直接渡す簡易実装とする
        // TODO: ConstantBuffer クラスをフレーム数分用意する

        // フレームインデックスを取得
        const auto frameIndex = m_GraphicsDevice.GetFrameIndex();

        // 変換行列を更新
        auto* pTransform = m_TransformCBs[frameIndex]->GetPtr<TransformCB>();
        pTransform->World = DirectX::XMMatrixRotationY(m_RotateAngle);
        pTransform->View = DirectX::XMMatrixLookAtRH(
            DirectX::XMVectorSet(0.0f, 0.0f, 200.0f, 0.0f),
            DirectX::XMVectorZero(),
            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
        pTransform->Proj = DirectX::XMMatrixPerspectiveFovRH(
            DirectX::XMConvertToRadians(45.0f),
            static_cast<float>(m_Width) / static_cast<float>(m_Height),
            0.1f, 1000.0f);

        // コマンドリストにセット（描画ループの前）
        pCmd->SetGraphicsRootConstantBufferView(
            ROOT_PARAM_CBV_TRANSFORM,
            m_TransformCBs[frameIndex]->GetAddress());

        // ─── メッシュの描画 ───
        for (auto& mesh : m_Meshes)
        {
            if (!mesh->IsValid())
            {
                continue;
            }

            const auto matId = mesh->GetMaterialId();
            if (matId < m_Materials.size())
            {
                // マテリアルの定数バッファをバインド
                pCmd->SetGraphicsRootConstantBufferView(
                    ROOT_PARAM_CBV_MATERIAL,
                    m_Materials[matId]->GetCBAddress());

                const auto texHandle = m_Materials[matId]->GetTextureHandle(Material::TEXTURE_DIFFUSE);

                // GPU ハンドルが有効な場合のみバインドする
                if (texHandle.ptr != 0)
                {
                    pCmd->SetGraphicsRootDescriptorTable(
                        ROOT_PARAM_SRV_DIFFUSE,
                        texHandle);
                }

                //// ディフューズテクスチャをバインド
                //pCmd->SetGraphicsRootDescriptorTable(
                //    ROOT_PARAM_SRV_DIFFUSE,
                //    m_Materials[matId]->GetTextureHandle(Material::TEXTURE_DIFFUSE));
            }

            // 描画コマンドを積む（内部で DrawIndexedInstanced が呼ばれる）
            mesh->Draw(pCmd);
        }
    }

    // ─── フレーム終了 ───
    // バリア・Close・Execute が内部で行われる
    m_GraphicsDevice.EndFrame();

    // ─── 画面表示 ───
    m_GraphicsDevice.Present(1);
}

// -------------------------------------------------------------------------------
// ウィンドウ初期化
// -------------------------------------------------------------------------------
bool Application::InitWnd()
{
    auto hInst = GetModuleHandle(nullptr);
    if (hInst == nullptr)
    {
        return false;
    }

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hIcon = LoadIcon(hInst, IDI_APPLICATION);
    wc.hCursor = LoadCursor(hInst, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
    wc.lpszClassName = WindowClassName;
    wc.hIconSm = LoadIcon(hInst, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        return false;
    }

    m_hInst = hInst;

    // クライアント領域が Width × Height になるようにウィンドウサイズを調整
    RECT rc = { 0, 0, static_cast<LONG>(m_Width), static_cast<LONG>(m_Height) };
    const auto style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRect(&rc, style, FALSE);

    m_hWnd = CreateWindowEx(
        0,
        WindowClassName,
        TEXT("DX12Framework"),
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        nullptr,
        m_hInst,
        nullptr);

    if (m_hWnd == nullptr)
    {
        return false;
    }

    ShowWindow(m_hWnd, SW_SHOWNORMAL);
    UpdateWindow(m_hWnd);
    SetFocus(m_hWnd);

    return true;
}

// -------------------------------------------------------------------------------
// ウィンドウ終了処理
// -------------------------------------------------------------------------------
void Application::TermWnd()
{
    if (m_hInst != nullptr)
    {
        UnregisterClass(WindowClassName, m_hInst);
    }

    m_hInst = nullptr;
    m_hWnd = nullptr;
}

// -------------------------------------------------------------------------------
// ウィンドウプロシージャ
// -------------------------------------------------------------------------------
LRESULT CALLBACK Application::WndProc(HWND _hWnd, UINT _msg, WPARAM _wp, LPARAM _lp)
{
    switch (_msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        break;
    }

    return DefWindowProc(_hWnd, _msg, _wp, _lp);
}

// -------------------------------------------------------------------------------
// RootSignature 生成
// -------------------------------------------------------------------------------
bool Application::InitRootSignature()
{
    auto* pDevice = m_GraphicsDevice.GetDevice();

    // ─── DescriptorRange（SRV: t0 ディフューズテクスチャ）───
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0; // register(t0)
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // ─── RootParameter ───
    D3D12_ROOT_PARAMETER params[3] = {};

    // [0] b0: 変換行列（頂点シェーダーで使用）
    params[ROOT_PARAM_CBV_TRANSFORM].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[ROOT_PARAM_CBV_TRANSFORM].Descriptor.ShaderRegister = 0;
    params[ROOT_PARAM_CBV_TRANSFORM].Descriptor.RegisterSpace = 0;
    params[ROOT_PARAM_CBV_TRANSFORM].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // [1] b1: マテリアルパラメータ（ピクセルシェーダーで使用）
    params[ROOT_PARAM_CBV_MATERIAL].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[ROOT_PARAM_CBV_MATERIAL].Descriptor.ShaderRegister = 1;
    params[ROOT_PARAM_CBV_MATERIAL].Descriptor.RegisterSpace = 0;
    params[ROOT_PARAM_CBV_MATERIAL].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // [2] t0: ディフューズテクスチャ（DescriptorTable、ピクセルシェーダーで使用）
    params[ROOT_PARAM_SRV_DIFFUSE].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[ROOT_PARAM_SRV_DIFFUSE].DescriptorTable.NumDescriptorRanges = 1;
    params[ROOT_PARAM_SRV_DIFFUSE].DescriptorTable.pDescriptorRanges = &srvRange;
    params[ROOT_PARAM_SRV_DIFFUSE].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ─── StaticSampler（LinearWrap、s0）───
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0; // register(s0)
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ─── RootSignature 設定 ───
    auto flag = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
    flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
    flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = _countof(params);
    desc.pParameters = params;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;
    desc.Flags = flag;

    ComPtr<ID3DBlob> pBlob;
    ComPtr<ID3DBlob> pErrorBlob;

    auto hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1_0,
        pBlob.GetAddressOf(),
        pErrorBlob.GetAddressOf());
    if (FAILED(hr))
    {
        if (pErrorBlob != nullptr)
        {
            ELOG("RootSignature serialize error: %s", (char*)pErrorBlob->GetBufferPointer());
        }
        return false;
    }

    hr = pDevice->CreateRootSignature(
        0,
        pBlob->GetBufferPointer(),
        pBlob->GetBufferSize(),
        IID_PPV_ARGS(m_pRootSignature.GetAddressOf()));
    if (FAILED(hr))
    {
        ELOG("CreateRootSignature failed.");
        return false;
    }

    return true;
}

// -------------------------------------------------------------------------------
// PSO 生成
// -------------------------------------------------------------------------------
bool Application::InitPipelineState()
{
    auto* pDevice = m_GraphicsDevice.GetDevice();

    // ─── シェーダーのロード ───
    ComPtr<ID3DBlob> pVSBlob;
    ComPtr<ID3DBlob> pPSBlob;

    std::wstring hlslPath;
    if(!SearchFilePath(L"MeshShaderVS.cso", hlslPath))
    {
        ELOG("hlsl FilePath found");
        return false;
    }
    auto hr = D3DReadFileToBlob(hlslPath.c_str(), pVSBlob.GetAddressOf());
    if (FAILED(hr))
    {
        ELOG("MeshShaderVS.cso not found.");
        return false;
    }

    if (!SearchFilePath(L"MeshShaderPS.cso", hlslPath))
    {
        ELOG("hlsl FilePath found");
        return false;
    }
    hr = D3DReadFileToBlob(hlslPath.c_str(), pPSBlob.GetAddressOf());
    if (FAILED(hr))
    {
        ELOG("MeshShaderPS.cso not found.");
        return false;
    }

    // ─── ラスタライザーステート ───
    D3D12_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rsDesc.CullMode = D3D12_CULL_MODE_BACK;    // 背面カリング有効
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rsDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rsDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rsDesc.DepthClipEnable = TRUE;
    rsDesc.MultisampleEnable = FALSE;
    rsDesc.AntialiasedLineEnable = FALSE;
    rsDesc.ForcedSampleCount = 0;
    rsDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // ─── ブレンドステート（アルファブレンドなし）───
    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    for (auto& rt : blendDesc.RenderTarget)
    {
        rt.BlendEnable = FALSE;
        rt.SrcBlend = D3D12_BLEND_ONE;
        rt.DestBlend = D3D12_BLEND_ZERO;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_ZERO;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rt.LogicOp = D3D12_LOGIC_OP_NOOP;
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    // ─── 深度ステンシルステート ───
    D3D12_DEPTH_STENCIL_DESC dssDesc = {};
    dssDesc.DepthEnable = TRUE;
    dssDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    dssDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    dssDesc.StencilEnable = FALSE;

    // ─── PSO 設定 ───
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = ResMeshVertex::InputLayout; // MeshLoader.cpp で定義
    psoDesc.pRootSignature = m_pRootSignature.Get();
    psoDesc.VS = { pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize() };
    psoDesc.PS = { pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize() };
    psoDesc.RasterizerState = rsDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState = dssDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    hr = pDevice->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(m_pPSO.GetAddressOf()));
    if (FAILED(hr))
    {
        ELOG("CreateGraphicsPipelineState failed. hr = 0x%08X", hr);
        return false;
    }

    return true;
}

// -------------------------------------------------------------------------------
// シーン（メッシュ・マテリアル）ロード
// -------------------------------------------------------------------------------
bool Application::InitScene()
{
    auto* pDevice = m_GraphicsDevice.GetDevice();
    auto* pQueue = m_GraphicsDevice.GetQueue();
    auto* pPool = m_GraphicsDevice.GetPool(GraphicsDevice::POOL_TYPE_RES);

    // ─── ファイルからロード ───
    std::vector<ResMesh>     resMeshes;
    std::vector<ResMaterial> resMaterials;

    std::wstring meshPath;
    /*if (!SearchFilePath(L"C:/DX12NextPlay/DX12Framework/Assets/Model/Elinyaa/Elinyaa.fbx", meshPath));
    {
        ELOG("Model filePath failed");
        return false;
    }*/
    if (!MeshLoader::Load(L"C:/DX12NextPlay/DX12Framework/Assets/Model/Elinyaa/Elinyaa.fbx", resMeshes, resMaterials))
    {
        ELOG("MeshLoader::Load() failed.");
        return false;
    }

    // ─── Mesh（GPU リソース）の生成 ───
    m_Meshes.reserve(resMeshes.size());
    for (auto& resMesh : resMeshes)
    {
        auto mesh = std::make_unique<Mesh>();
        if (!mesh->Init(pDevice, resMesh))
        {
           // ELOG("Mesh::Init() failed. index = %u", i);
            return false;
        }
        m_Meshes.emplace_back(std::move(mesh));
    }

    // ─── Material（GPU リソース）の生成 ───
    m_Materials.reserve(resMaterials.size());
    for (auto& resMat : resMaterials)
    {
        auto material = std::make_unique<Material>();
        if (!material->Init(pDevice, pQueue, pPool, resMat))
        {
            return false;
        }
        m_Materials.emplace_back(std::move(material));
    }

    // 変換行列の定数バッファを FrameCount 分生成
    const auto frameCount = m_GraphicsDevice.GetFrameCount();
    m_TransformCBs.reserve(frameCount);
    for (auto i = 0u; i < frameCount; ++i)
    {
        auto cb = std::make_unique<ConstantBuffer>();
        if (!cb->Init(pDevice, pPool, sizeof(TransformCB)))
        {
            ELOG("TransformCB::Init() failed.");
            return false;
        }
        m_TransformCBs.emplace_back(std::move(cb));
    }

    return true;
}