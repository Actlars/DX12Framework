// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Application.h"
#include <Engine/Utility/Debug/Logger/Logger.h>
#include <Game/Scene/GameScene/GameScene.h>

// -------------------------------------------------------------------------------
// 定数
// -------------------------------------------------------------------------------
namespace
{
    constexpr auto WindowClassName = TEXT("DX12FrameworkWindow");

    // コンテンツブラウザが表示するフォルダ
    // 実行時に無ければ自動で作られる
    constexpr auto ContentRootPath = L"C:/DX12NextPlay/DX12Framework/Assets/Content";

    // -------------------------------------------------------------------------------
    // EditorUIのキーと、Windowsの仮想キーコードの対応表
    //
    // EditorUI側はWindowsのキーコードを知らない設計にしてあるため、
    // その橋渡しをプラットフォーム層であるApplicationが受け持つ
    // 修飾キーは左右を区別しないVK_を使い、どちらを押しても同じ扱いにする
    // -------------------------------------------------------------------------------
    struct EditorKeyBinding
    {
        EditorUI::Key   Key;
        int             VirtualKey;
    };

    constexpr EditorKeyBinding kEditorKeyBindings[] =
    {
        { EditorUI::Key::Backspace, VK_BACK      },
        { EditorUI::Key::Delete,    VK_DELETE    },
        { EditorUI::Key::Left,      VK_LEFT      },
        { EditorUI::Key::Right,     VK_RIGHT     },
        { EditorUI::Key::Up,        VK_UP        },
        { EditorUI::Key::Down,      VK_DOWN      },
        { EditorUI::Key::Home,      VK_HOME      },
        { EditorUI::Key::End,       VK_END       },
        { EditorUI::Key::Enter,     VK_RETURN    },
        { EditorUI::Key::Escape,    VK_ESCAPE    },
        { EditorUI::Key::Tab,       VK_TAB       },
        { EditorUI::Key::Ctrl,      VK_CONTROL   },
        { EditorUI::Key::Shift,     VK_SHIFT     },
        { EditorUI::Key::Alt,       VK_MENU      },
        { EditorUI::Key::A,         'A'          },
    };

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
//
// 順序に意味があるものだけを列挙すると
//	1. ウィンドウ → デバイス		描画先が決まってからでないとデバイスを作れない
//	2. シーン						テクスチャ転送でコマンドキューを使う
//	3. EditorUIレンダラ → フォント	フォントはアトラスの登録先を必要とする
//	4. ビューポート → EditorApp		EditorAppはビューポートを参照する
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

    // ─── EditorUI ───
    m_EditorUIRenderer.Init(m_GraphicsView.GetDevice(), 2);

    const EditorUI::Rect2D screenBounds = EditorUI::MakeRect(
        { 0.0f,0.0f }, { static_cast<float>(m_Width),static_cast<float>(m_Height) });
    m_EditorUIContext.InitDockSpace(screenBounds);

    if (!m_Font.Build(L"Yu Gothic UI", 16.0f, m_GraphicsView.GetDevice(), &m_EditorUIRenderer))
    {
        ELOG("Application::Init() : Font::Build failed");
        return false;
    }

    // タイトルバーとドックタブの文字はContext自身が描くため、フォントを共有する
    m_EditorUIContext.SetFont(&m_Font);

    // ─── ゲーム画面の描画先 ───
    // 実際のリソースは、ビューポートパネルの大きさが決まった時点で確保される
    if (!m_ViewportTarget.Init(m_GraphicsView.GetDevice(), &m_EditorUIRenderer))
    {
        ELOG("Application::Init() : ViewportTarget::Init failed");
        return false;
    }

    // ─── エディタ本体 ───
    if (!m_EditorApp.Init(
        &m_EditorUIContext, &m_Font, &m_ViewportTarget, &m_SceneManager, ContentRootPath))
    {
        ELOG("Application::Init() : EditorApp::Init failed");
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

    m_EditorApp.Term();

    // InputManagerの終了
    m_InputManager.Term();

    // SceneManagerを先に終了する（GPUリソースを持つので、GraphicsDeviceより前）
    m_SceneManager.Term();

    // ビューポートのリソースもデバイスより前に解放する
    m_ViewportTarget.Term();

    m_EditorUIRenderer.Term();

    m_Font.Term();

    // RendererとDeviceの終了を内部で行う
    m_GraphicsView.Term();

    TermWnd();
}

// -------------------------------------------------------------------------------
// 更新処理と描画処理
//
// 1フレームの流れ
//	1. 入力更新
//	2. エディタUIの構築（ここでビューポートに必要な大きさが決まる）
//	3. 入力の割り当て（UIとカメラのどちらがマウスを持つか）
//	4. シーン更新
//	5. 描画  ゲーム画面 → ビューポート用テクスチャ、その後 EditorUI → バックバッファ
// -------------------------------------------------------------------------------
void Application::Tick()
{
    // Escで終了
    // テキスト編集中のEscapeは「入力の取り消し」なので、アプリの終了には使わない
    if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) && !m_EditorUIContext.WantCaptureKeyboard())
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

    // エディタUIの構築
    UpdateEditorUI(deltaTime);

    m_Font.Flush(m_GraphicsView.GetDevice());

    // -------------------------------------------------------------------------------
    // マウスの持ち主を決める
    //
    // カメラを動かしてよいのは「ゲーム画面の上にいて、UIの操作中でもない」ときだけ
    // ゲーム画面もウィンドウの一種になったため、
    // 「どこかのウィンドウの上にいるか」だけでは判断できなくなっている
    // -------------------------------------------------------------------------------
    const bool sceneOwnsMouse =
        m_EditorApp.IsViewportHovered() && !m_EditorUIContext.IsAnyItemActive();

    m_InputManager.ResolveMouseCapture(!sceneOwnsMouse);

    // シーンの更新
    m_SceneManager.Update(deltaTime);

    // 描画
    auto* pCmdVoid = m_GraphicsView.BeginFrame();
    if (pCmdVoid != nullptr)
    {
        // SceneManagerにvoid型を渡しても使えないので、ここでだけキャストする
        auto* pCmd = static_cast<ID3D12GraphicsCommandList*>(pCmdVoid);

        // ゲーム画面はバックバッファではなくビューポート用テクスチャへ描く
        const bool renderedViewport = RenderSceneToViewport(pCmd);

        // ビューポートへ描いた場合、描画先とビューポート設定が切り替わっている
        // EditorUIをバックバッファへ描くために、明示的に戻す
        if (renderedViewport)
        {
            auto* pRTV = m_GraphicsView.GetDevice()->GetColorTarget(
                m_GraphicsView.GetFrameIndex())->GetHandleRTV();

            pCmd->OMSetRenderTargets(1, &pRTV->HandleCPU, FALSE, nullptr);
        }

        m_EditorUIRenderer.Render(
            m_EditorUIContext.GetFrameOutput(), pCmd,
            m_GraphicsView.GetFrameIndex(),
            static_cast<float>(m_Width), static_cast<float>(m_Height));

        m_GraphicsView.EndFrame(pCmdVoid);

        // 画面表示
        m_GraphicsView.Present(1);
    }

    // シーン切り替え処理（遅延）
    m_SceneManager.ProcessSceneChange();
}

// -------------------------------------------------------------------------------
// エディタUIの構築
//
// 描画より前に行う必要がある
//	- ビューポートに必要な大きさが、このフレームのレイアウトで決まるため
//	- ドッキング状態が確定してからでないと、各ウィンドウの矩形が定まらないため
// -------------------------------------------------------------------------------
void Application::UpdateEditorUI(float _deltaTime)
{
    const EditorUI::Rect2D screenBounds = EditorUI::MakeRect(
        { 0.0f,0.0f },
        { static_cast<float>(m_Width), static_cast<float>(m_Height) });

    m_EditorUIContext.UpdateDockSpaceLayout(screenBounds);

    const EditorUI::InputState input = PollInputState(_deltaTime);
    m_EditorUIContext.NewFrame(input);

    // どんなパネルを出すかはEditorAppが決める
    m_EditorApp.BuildUI(_deltaTime);

    m_EditorUIContext.EndFrame();
}

// -------------------------------------------------------------------------------
// ゲーム画面をビューポート用テクスチャへ描く
// -------------------------------------------------------------------------------
bool Application::RenderSceneToViewport(ID3D12GraphicsCommandList* _pCmd)
{
    uint32_t width  = 0;
    uint32_t height = 0;

    // ビューポートが閉じている・折り畳まれている場合はここで終わる
    // 見えていないものを描くコストを払わずに済む
    if (!m_EditorApp.GetRequestedViewportSize(width, height))
    {
        return false;
    }

    // 大きさが変わったときだけ作り直される
    if (!m_ViewportTarget.Resize(width, height))
    {
        return false;
    }

    // シーンの描画先をビューポートへ切り替える
    // カメラの縦横比も、この指定に合わせて更新される
    m_SceneManager.SetSceneOutput(m_ViewportTarget.GetSceneOutput());

    m_ViewportTarget.Begin(_pCmd);
    m_SceneManager.Render(_pCmd);
    m_ViewportTarget.End(_pCmd);

    return true;
}

// -------------------------------------------------------------------------------
// 入力のスナップショットを作る
// -------------------------------------------------------------------------------
EditorUI::InputState Application::PollInputState(float _deltaTime) const
{
    EditorUI::InputState input;

    // ダブルクリックの判定やエフェクトの再生に使う
    input.DeltaTime = _deltaTime;

    // -------------------------------------------------------------------------------
    // マウス
    // -------------------------------------------------------------------------------
    const Input::MouseInput& mouseInput = m_InputManager.GetMouseInput();

    const POINT position = mouseInput.GetPosition();
    input.MousePos = { static_cast<float>(position.x), static_cast<float>(position.y) };

    input.MouseDown[EditorUI::ToIndex(EditorUI::MouseButton::Mouse_Left)]   = mouseInput.IsDown(Input::MouseButton::Left);
    input.MouseDown[EditorUI::ToIndex(EditorUI::MouseButton::Mouse_Right)]  = mouseInput.IsDown(Input::MouseButton::Right);
    input.MouseDown[EditorUI::ToIndex(EditorUI::MouseButton::Mouse_Middle)] = mouseInput.IsDown(Input::MouseButton::Middle);

    input.MouseWheel = mouseInput.GetWheelDelta();

    // -------------------------------------------------------------------------------
    // キーボード
    //
    // KeyDownはポーリング結果、KeyPressedはWM_KEYDOWNの通知から取る
    // 後者はオートリピートを含むため、BackSpaceの押しっぱなしが正しく連続する
    // -------------------------------------------------------------------------------
    const Input::KeyboardInput& keyboardInput = m_InputManager.GetKeyboardInput();

    for (const EditorKeyBinding& binding : kEditorKeyBindings)
    {
        const std::size_t index = EditorUI::ToIndex(binding.Key);

        input.KeyDown[index]    = keyboardInput.IsDown(static_cast<uint32_t>(binding.VirtualKey));
        input.KeyPressed[index] = keyboardInput.HasKeyDownEvent(static_cast<uint32_t>(binding.VirtualKey));
    }

    input.InputCharacters = keyboardInput.GetInputCharacters();

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

    // 最後の引数(lpParam)でthisを渡す
    // WndProcはstaticでインスタンスを持てないため、WM_NCCREATEでこれを受け取り
    // ウィンドウ自身に結び付けることで、以降のメッセージから自分を辿れるようにする
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
        this);

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
// 入力に関わるメッセージの配布
// -------------------------------------------------------------------------------
void Application::HandleWindowMessage(UINT _msg, WPARAM _wp, LPARAM _lp)
{
    // ホイール量・確定文字・キーの押下通知は、ポーリングでは取得できない
    // メッセージが届いたこの場でInputManagerへ渡し、次のUpdateで確定させる
    m_InputManager.ProcessWindowMessage(_msg, _wp, _lp);
}

// -------------------------------------------------------------------------------
// ウィンドウプロシージャ
// -------------------------------------------------------------------------------
LRESULT CALLBACK Application::WndProc(HWND _hWnd, UINT _msg, WPARAM _wp, LPARAM _lp)
{
    // CreateWindowExに渡したthisを、最初のメッセージでウィンドウ自身へ結び付ける
    // 以降はGetWindowLongPtrでインスタンスを取り出せる
    if (_msg == WM_NCCREATE)
    {
        const auto* pCreate = reinterpret_cast<const CREATESTRUCT*>(_lp);
        SetWindowLongPtr(_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreate->lpCreateParams));
    }

    auto* pApp = reinterpret_cast<Application*>(GetWindowLongPtr(_hWnd, GWLP_USERDATA));
    if (pApp != nullptr)
    {
        pApp->HandleWindowMessage(_msg, _wp, _lp);
    }

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
