// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Application.h"
#include <Engine/Utility/Debug/Logger/Logger.h>
#include <Game/Scene/GameScene/GameScene.h>
#include <Engine/EditorUI/Widgets/Widgets.h>
#include <Engine/EditorUI/Widgets/Layout/Layout.h>

// -------------------------------------------------------------------------------
// 定数
// -------------------------------------------------------------------------------
namespace
{
    constexpr auto WindowClassName = TEXT("DX12FrameworkWindow");

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
    /*RHI::Device::Desc gfxDesc;
    gfxDesc.hWnd        = m_hWnd;
    gfxDesc.Width       = m_Width;
    gfxDesc.Height      = m_Height;
    gfxDesc.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
    gfxDesc.DepthFormat = DXGI_FORMAT_D32_FLOAT;
    gfxDesc.FrameCount  = 2;*/

    m_GraphicsView.Init(m_hWnd, m_Width, m_Height);

    // SceneManagerの初期化
    m_SceneManager.Init(m_GraphicsView.GetDevice());

    // InputManagerの初期化
    if (!m_InputManager.Init(m_hWnd))
    {
        ELOG("InputManager::Init() failed");
        return false;
    }

    // ─── 最初のシーンを登録して即時切り替え ───
    // 遅延切り替えだが最初のシーンは即時適用する
    GameScene::Desc sceneDesc;
    sceneDesc.ModelPath         = L"C:/DX12NextPlay/DX12Framework/Assets/Model/Elinyaa/Elinyaa.fbx";
    sceneDesc.pInputManager     = &m_InputManager;
    sceneDesc.CameraPosition    = { 0.0f, 1.0f, -5.0f };
    sceneDesc.CameraMoveSpeed   = 10.0f;
    sceneDesc.CameraRotSpeed    = 0.2f;
    sceneDesc.CameraFov         = 60.0f;
    sceneDesc.CameraNear        = 0.1f;
    sceneDesc.CameraFar         = 10000.0f;

    m_SceneManager.ChangeScene(std::make_unique<GameScene>(sceneDesc));
    m_SceneManager.ProcessSceneChange(); // 最初のシーンは即時切り替え

    // ─── タイマー初期化 ───
    QueryPerformanceFrequency(&m_Frequency);
    QueryPerformanceCounter(&m_PrevTime);

    // シーン初期化中にテクスチャアップロードでキューを使うため
    // フレームインデックスを最新状態に同期する
    m_GraphicsView.UpdateFrameIndexAfterSceneChange();

    m_EditorUIRenderer.Init(m_GraphicsView.GetDevice(), 2);

    if (!m_Font.Build(L"Yu Gothic UI", 16.0f, m_GraphicsView.GetDevice(), &m_EditorUIRenderer))
    {
        ELOG("Application::Init() : Font::Build failed");
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
            Tick();
        }
    }
}

// -------------------------------------------------------------------------------
// 終了処理
// -------------------------------------------------------------------------------
void Application::Term()
{
    // GPUの全処理が完了するのを待機する
    m_GraphicsView.WaitForGPU();

    // InputManagerの終了
    m_InputManager.Term();

    // SceneManagerを先に終了する（GPUリソースを持つので、GraphicsDeviceより前）
    m_SceneManager.Term();

    m_EditorUIRenderer.Term();

    m_Font.Term();

    // RendererとDeviceの終了を内部で行う
    m_GraphicsView.Term();

    TermWnd();
}

// -------------------------------------------------------------------------------
// 更新処理と描画処理
// -------------------------------------------------------------------------------
void Application::Tick()
{
    // Escで終了
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
    {
        PostQuitMessage(0);
        return;
    }

    // デルタタイム計算
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const float deltaTime = static_cast<float>(now.QuadPart - m_PrevTime.QuadPart)
                            / static_cast<float>(m_Frequency.QuadPart);

    m_PrevTime = now;

    // 入力更新
    m_InputManager.Update();

    // UI開始
    EditorUI::InputState input = PollInputState();
    m_EditorUIContext.NewFrame(input);

    static bool s_TestWindowOpen = true;

    if (s_TestWindowOpen)
    {
        const bool showContents = m_EditorUIContext.BeginWindow("Test Window", &s_TestWindowOpen);

        static bool s_Enabled = false;
        if (showContents)
        {
            // ボタン処理など
            if (EditorUI::Button(m_EditorUIContext, "OK", m_Font))
            {
                ELOG("Button OK clicked");
            }

            EditorUI::Checkbox(m_EditorUIContext, "Enable Feature", &s_Enabled);
            EditorUI::Separator(m_EditorUIContext);

            EditorUI::Button(m_EditorUIContext, "A", m_Font);
            EditorUI::SameLine(m_EditorUIContext.GetCurrentWindow(), m_EditorUIContext.GetStyle().ItemSpacing);
            EditorUI::Button(m_EditorUIContext, "B", m_Font);
        }

        m_EditorUIContext.EndWindow();
    }

    // UI構築終了
    m_EditorUIContext.EndFrame();

    m_Font.Flush(m_GraphicsView.GetDevice());

    // UI上でクリックしたか、シーン上でクリックしたかを決定
    m_InputManager.ResolveMouseCapture(m_EditorUIContext.IsMouseOverAnyWindow());

    // シーンの更新
    m_SceneManager.Update(deltaTime);

    // 描画
    auto* pCmdVoid = m_GraphicsView.BeginFrame();
    if (pCmdVoid != nullptr)
    {
        // SceneManagerにvoid型を渡しても使えないので、ここでだけキャストする
        auto* pCmd = static_cast<ID3D12GraphicsCommandList*>(pCmdVoid);
        m_SceneManager.Render(pCmd);

        m_EditorUIRenderer.Render(
            m_EditorUIContext.GetCompositedFrame(), pCmd,
            m_GraphicsView.GetFrameIndex(),
            static_cast<float>(m_Width), static_cast<float>(m_Height));

        m_GraphicsView.EndFrame(pCmdVoid);

        // 画面表示
        m_GraphicsView.Present(1);
    }

    // シーン切り替え処理（遅延）
    m_SceneManager.ProcessSceneChange();
}

EditorUI::InputState Application::PollInputState() const
{
    EditorUI::InputState input;

    const Input::MouseInput& mouseInput = m_InputManager.GetMouseInput();

    const POINT Position = mouseInput.GetPosition();
    input.MousePos = { static_cast<float>(Position.x), static_cast<float>(Position.y) };

    input.MouseDown[static_cast<int>(EditorUI::MouseButton::Mouse_Left)] =      mouseInput.IsDown(Input::MouseButton::Left);
    input.MouseDown[static_cast<int>(EditorUI::MouseButton::Mouse_Right)] =     mouseInput.IsDown(Input::MouseButton::Right);
    input.MouseDown[static_cast<int>(EditorUI::MouseButton::Mouse_Middle)] =    mouseInput.IsDown(Input::MouseButton::Middle);
    
    return input;
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
    wc.cbSize           = sizeof(WNDCLASSEX);
    wc.style            = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc      = WndProc;
    wc.hIcon            = LoadIcon(hInst, IDI_APPLICATION);
    wc.hCursor          = LoadCursor(hInst, IDC_ARROW);
    wc.hbrBackground    = GetSysColorBrush(COLOR_BACKGROUND);
    wc.lpszClassName    = WindowClassName;
    wc.hIconSm          = LoadIcon(hInst, IDI_APPLICATION);

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
    if (m_hWnd != nullptr)
    {
        DestroyWindow(m_hWnd);
    }

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
