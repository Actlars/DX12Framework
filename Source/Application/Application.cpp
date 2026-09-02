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
    // プロジェクトのルート
    //
    // モデルやシーンのパスは、ここからの相対で扱う
    // ファイルへ絶対パスを書くと、プロジェクトを別の場所へ移した時点で開けなくなるため
    // -------------------------------------------------------------------------------
    constexpr auto ProjectRootPath = L"C:/DX12NextPlay/DX12Framework";

    // -------------------------------------------------------------------------------
    // ウィンドウの位置と大きさの保存先
    //
    // アセットではなく利用者ごとの設定なので、Contentとは別のフォルダへ置く
    // 無ければ終了時に自動で作られる
    // -------------------------------------------------------------------------------
    constexpr auto WindowSettingsPath = L"C:/DX12NextPlay/DX12Framework/Config/Window.json";

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
        { EditorUI::Key::D,         'D'          },
        { EditorUI::Key::Y,         'Y'          },
        { EditorUI::Key::Z,         'Z'          },
        { EditorUI::Key::F2,        VK_F2        },
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
//	4. ビューポート → EditorApp	EditorAppはビューポートを参照する
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

    // ─── モデルの置き場 ───
    // シーンより先に用意する。シーンの初期化中に読み込みが走るため
    if (!m_ModelLibrary.Init(m_GraphicsView.GetDevice(), ProjectRootPath))
    {
        ELOG("Application::Init() : ModelLibrary::Init failed");
        return false;
    }

    // SceneManagerの初期化
    m_SceneManager.Init(m_GraphicsView.GetDevice(), &m_ModelLibrary);

    // InputManagerの初期化
    if (!m_InputManager.Init(m_hWnd))
    {
        ELOG("InputManager::Init() failed");
        return false;
    }

    // ─── 最初のシーンを登録して即時切り替え ───
    // 遅延切り替えだが最初のシーンは即時適用する
    GameScene::Desc sceneDesc;
    // プロジェクトからの相対パスで指定する（ModelLibraryが絶対パスへ直す）
    sceneDesc.ModelPath         = L"Assets/Model/Player/Elinyaa/Elinyaa.fbx";
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

    // フォントをGPUTextureにアップロードする
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

    // エディタ本体の作成
    if (!m_EditorApp.Init(
        &m_EditorUIContext, &m_Font, &m_ViewportTarget, &m_SceneManager,
        m_GraphicsView.GetDevice(), &m_EditorUIRenderer, &m_ModelLibrary, ContentRootPath))
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

    // シーンが参照し終わってから、モデルの実体を解放する
    m_ModelLibrary.Term();

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

    // -------------------------------------------------------------------------------
    // ウィンドウサイズの変更をここで反映する
    //
    // 描画にもUIのレイアウトにも関わるため、フレームの一番最初に済ませる
    // -------------------------------------------------------------------------------
    ApplyPendingResize();

    // 最小化中は描画先が存在しないため、フレームごと省く
    if (m_IsMinimized)
    {
        return;
    }

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
    // すでにカメラが操作権を持っている場合は、離すまで維持する
    // 相対モード中はカーソルが画面中央へ戻されるため、
    // 「ビューポートの上にいるか」だけで判断すると途中で操作権を失ってしまう
    const bool sceneOwnsMouse =
        m_InputManager.IsCameraControlActive() || m_EditorApp.IsViewportHovered();

    m_InputManager.ResolveMouseCapture(!sceneOwnsMouse);

    // シーンの更新
    m_SceneManager.Update(deltaTime);

    // 描画
    auto* pCmdVoid = m_GraphicsView.BeginFrame();
    if (pCmdVoid != nullptr)
    {
        // SceneManagerにvoid型を渡しても使えないので、ここでだけキャストする
        auto* pCmd = static_cast<ID3D12GraphicsCommandList*>(pCmdVoid);

        // -------------------------------------------------------------------------------
        // 自前のレンダーターゲットを持つパネル（エフェクトのプレビュー等）を先に描く
        //
        // どれもバックバッファとは別の描画先を使うため、
        // ゲーム画面やUIより前に済ませておくのがいちばん素直になる
        // -------------------------------------------------------------------------------
        m_EditorApp.RenderPanels(pCmd);

        // ゲーム画面はバックバッファではなくビューポート用テクスチャへ描く
        const bool renderedViewport = RenderSceneToViewport(pCmd);

        // ビューポートへ描いた場合、描画先とビューポート設定が切り替わっている
        // EditorUIをバックバッファへ描くために、明示的に戻す
        // パネルのプレビューを描いた場合も描画先が変わっているため、必ず戻す
        if (true)
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
    // 左上を(0,0)とし、現在のClient領域全体をScreenBoundsとして扱う。
    const EditorUI::Rect2D screenBounds = EditorUI::MakeRect(
        { 0.0f,0.0f },
        { static_cast<float>(m_Width), static_cast<float>(m_Height) });

    // -------------------------------------------------------------------------------
    // DockSpaceを現在の画面サイズへ追従させる
    // -------------------------------------------------------------------------------

    // NewFrameによるHover/Splitter判定より前にDockNodeのBoundsを更新する
    // ここではDockTreeそのものを作り直すのではなく、
    // 現在保持しているSplitRatioを維持したまま各Nodeの矩形だけを再計算
    m_EditorUIContext.UpdateDockSpaceLayout(screenBounds);

    // -------------------------------------------------------------------------------
    // 今フレームの入力状態を取得
    // -------------------------------------------------------------------------------

    // Application側で保持しているMouse・Keyboard入力をEditorUIが扱えるInputStateへまとめる
    const EditorUI::InputState input = PollInputState(_deltaTime);

    // -------------------------------------------------------------------------------
    // EditorUIのフレーム開始処理
    // -------------------------------------------------------------------------------

    // NewFrameでは主に、
    // ・Clicked/Releasedなど今フレームの入力状態確定
    // ・遅延Window削除の反映
    // ・Widget/Window/DockのInteraction状態更新
    // ・前フレームで確定したWindow矩形を利用したHover/Focus判定
    // ・Dock中のSplitterやWindow移動処理終了
    // を行う
    m_EditorUIContext.NewFrame(input);

    
    // -------------------------------------------------------------------------------
    // Editor本体のWindow/Panel/Widgetを構築
    // -------------------------------------------------------------------------------

    // Hierarchy/Inspector/Viewport/ContentBrowserなど、
    // 実際にどのWindowをBeginして何を描画するかはEditorAppへ移譲する
    m_EditorApp.BuildUI(_deltaTime);

    // -------------------------------------------------------------------------------
    // EditorUIのフレーム終了処理
    // -------------------------------------------------------------------------------

    // 各Windowが生成したDrawListを最終的な描画順へ並べる
    // 基本的なz-orderは、
    // DockWindow → FloatingWindow → Popup → Overlayの順に並べ替える
    // ここで完了したFrameOutputを、後段のEditorUIRendererがGPUへ描画する
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
    //
    // カメラ操作中はカーソルが毎フレーム画面中央へ戻される
    // その座標をそのままUIへ渡すと、画面中央のウィジェットが反応してしまうため、
    // 「画面外にいる」ことにして入力を届けない
    // -------------------------------------------------------------------------------
    const Input::MouseInput& mouseInput = m_InputManager.GetMouseInput();

    if (m_InputManager.IsCameraControlActive())
    {
        input.MousePos = { -1.0f, -1.0f };
        input.InputCharacters.clear();
        return input;
    }

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
    // -------------------------------------------------------------------------------
    // 既定のマウスカーソル
    //
    //  第1引数はnullptrにする
    //  モジュールハンドルを渡すと「このexeの中にある、その名前のカーソル資源」を
    //  探しに行って見つからず、NULLが入る
    //  hCursorがNULLのウィンドウはカーソルの形をOSに任せきりになるため、
    //  ウィンドウ枠でリサイズ矢印になったあと、中へ入っても矢印のまま戻らなくなる
    // -------------------------------------------------------------------------------
    wc.hCursor          = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground    = GetSysColorBrush(COLOR_BACKGROUND);
    wc.lpszClassName    = WindowClassName;
    wc.hIconSm          = LoadIcon(hInst, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        return false;
    }

    m_hInst = hInst;

    // -------------------------------------------------------------------------------
    // ウィンドウスタイル
    //
    //  WS_OVERLAPPEDWINDOW は次をまとめて含む
    //      WS_CAPTION      タイトルバー
    //      WS_SYSMENU      システムメニュー（右クリックで出るメニュー）
    //      WS_THICKFRAME   ドラッグでサイズを変えられる枠
    //      WS_MINIMIZEBOX  最小化ボタン
    //      WS_MAXIMIZEBOX  最大化ボタン
    //
    //  以前は WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU だったため、
    //  枠を掴んでも伸ばせず、最大化ボタンも出ていなかった
    // -------------------------------------------------------------------------------
    RECT rc = { 0, 0, static_cast<LONG>(m_Width), static_cast<LONG>(m_Height) };
    const auto style = WS_OVERLAPPEDWINDOW;
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

    // -------------------------------------------------------------------------------
    // 前回終了時の位置と大きさを反映する
    //
    // ShowWindowより前に行うことで、既定サイズで一瞬表示されてから
    // 動くというちらつきを避ける
    // -------------------------------------------------------------------------------
    RestoreWindowPlacement();

    ShowWindow(m_hWnd, m_WindowSettings.Maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
    UpdateWindow(m_hWnd);
    SetFocus(m_hWnd);

    return true;
}

// -------------------------------------------------------------------------------
// ウィンドウ終了処理
// -------------------------------------------------------------------------------
void Application::TermWnd()
{
    // -------------------------------------------------------------------------------
    // 位置と大きさを覚えてから閉じる
    //
    // ウィンドウを壊したあとでは取得できないため、必ずDestroyWindowより前に行う
    // -------------------------------------------------------------------------------
    SaveWindowPlacement();

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
// EditorUIが希望する形へマウスカーソルを切り替える
//
// EditorUIはWindowsのAPIを知らないため、形の希望を持つだけで実際には変えられない
// その希望をWin32のカーソルへ翻訳するのが、プラットフォーム層であるここの役目
// -------------------------------------------------------------------------------
void Application::ApplyMouseCursor() const
{
    LPCTSTR cursorName = IDC_ARROW;

    switch (m_EditorUIContext.GetMouseCursor())
    {
    case EditorUI::MouseCursor::TextInput:   cursorName = IDC_IBEAM;     break;
    case EditorUI::MouseCursor::ResizeNWSE:  cursorName = IDC_SIZENWSE;  break;
    case EditorUI::MouseCursor::Hand:        cursorName = IDC_HAND;      break;

    case EditorUI::MouseCursor::Arrow:
    default:                                 cursorName = IDC_ARROW;     break;
    }

    SetCursor(LoadCursor(nullptr, cursorName));
}

// -------------------------------------------------------------------------------
// 前回終了時のウィンドウ配置を復元する
// -------------------------------------------------------------------------------
void Application::RestoreWindowPlacement()
{
    if (m_hWnd == nullptr)
    {
        return;
    }

    if (!m_WindowSettings.Load(WindowSettingsPath))
    {
        return;	// 初回起動、または内容が使えない
    }

    // -------------------------------------------------------------------------------
    // 画面の外に出ていないかを確かめる
    //
    // モニタを外した後などに前回の位置をそのまま使うと、
    // 見えない場所にウィンドウが開いて操作できなくなる
    // 少しでも見えている位置なら、そのまま使ってよい
    // -------------------------------------------------------------------------------
    const RECT bounds =
    {
        m_WindowSettings.PositionX,
        m_WindowSettings.PositionY,
        m_WindowSettings.PositionX + m_WindowSettings.Width,
        m_WindowSettings.PositionY + m_WindowSettings.Height
    };

    if (MonitorFromRect(&bounds, MONITOR_DEFAULTTONULL) == nullptr)
    {
        // どのモニタにも重なっていないため、既定の位置のままにする
        // 最大化の指定だけは活かす（どのモニタでも意味が通るため）
        return;
    }

    SetWindowPos(
        m_hWnd,
        nullptr,
        m_WindowSettings.PositionX,
        m_WindowSettings.PositionY,
        m_WindowSettings.Width,
        m_WindowSettings.Height,
        SWP_NOZORDER | SWP_NOACTIVATE);

    // -------------------------------------------------------------------------------
    // クライアント領域の大きさを、実際の値へ合わせておく
    //
    // このあとGraphicsDeviceがこの値でスワップチェインを作るため、
    // ここでずれていると初回だけ表示が引き伸ばされる
    // -------------------------------------------------------------------------------
    RECT clientRect = {};
    if (GetClientRect(m_hWnd, &clientRect))
    {
        const uint32_t width  = static_cast<uint32_t>(clientRect.right  - clientRect.left);
        const uint32_t height = static_cast<uint32_t>(clientRect.bottom - clientRect.top);

        if (width > 0 && height > 0)
        {
            m_Width  = width;
            m_Height = height;
        }
    }
}

// -------------------------------------------------------------------------------
// 今のウィンドウ配置を保存する
// -------------------------------------------------------------------------------
void Application::SaveWindowPlacement() const
{
    if (m_hWnd == nullptr)
    {
        return;
    }

    // -------------------------------------------------------------------------------
    // GetWindowPlacementを使う理由
    //
    // GetWindowRectは「今見えている矩形」を返すため、最大化中は画面いっぱいになる
    // その値を保存すると、次の起動で最大化を解除したときに
    // 画面いっぱいの大きさのままになってしまう
    // GetWindowPlacementなら「元に戻したときの矩形」と最大化状態を別々に取得できる
    // -------------------------------------------------------------------------------
    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);

    if (!GetWindowPlacement(m_hWnd, &placement))
    {
        return;
    }

    WindowSettings settings;

    settings.PositionX	= placement.rcNormalPosition.left;
    settings.PositionY	= placement.rcNormalPosition.top;
    settings.Width		= placement.rcNormalPosition.right  - placement.rcNormalPosition.left;
    settings.Height		= placement.rcNormalPosition.bottom - placement.rcNormalPosition.top;

    // 最小化した状態で終了した場合は、次回まで持ち越さない
    // 最小化のまま開き直しても操作できないため
    settings.Maximized	= (placement.showCmd == SW_SHOWMAXIMIZED);

    if (!settings.IsValid())
    {
        return;
    }

    settings.Save(WindowSettingsPath);
}

// -------------------------------------------------------------------------------
// 入力に関わるメッセージの配布
// -------------------------------------------------------------------------------
void Application::HandleWindowMessage(UINT _msg, WPARAM _wp, LPARAM _lp)
{
    // ホイール量・確定文字・キーの押下通知は、ポーリングでは取得できない
    // メッセージが届いたこの場でInputManagerへ渡し、次のUpdateで確定させる
    m_InputManager.ProcessWindowMessage(_msg, _wp, _lp);

    switch (_msg)
    {
    case WM_SIZE:
    {
        // -------------------------------------------------------------------------------
        // 新しいクライアント領域の大きさを控えるだけにする
        //
        // ここでバックバッファを作り直すと、描画の途中で差し替わる恐れがある
        // 実際の反映は次のフレームの先頭（ApplyPendingResize）で行う
        // -------------------------------------------------------------------------------
        m_IsMinimized = (_wp == SIZE_MINIMIZED);

        const uint32_t width  = static_cast<uint32_t>(LOWORD(_lp));
        const uint32_t height = static_cast<uint32_t>(HIWORD(_lp));

        if (!m_IsMinimized && width > 0 && height > 0)
        {
            m_PendingWidth   = width;
            m_PendingHeight  = height;
            m_ResizePending  = true;
        }
        break;
    }

    case WM_SETCURSOR:
        // クライアント領域の中だけ、こちらでカーソルの形を決める
        // 枠の上（HTLEFTなど）はOSに任せたほうが、実際の操作と形が一致する
        if (LOWORD(_lp) == HTCLIENT)
        {
            ApplyMouseCursor();
        }
        break;

    case WM_CLOSE:
        // -------------------------------------------------------------------------------
        // 閉じる直前に、位置と大きさを覚えておく
        //
        // このあとDefWindowProcがウィンドウを破棄するため、
        // 終了処理まで待つと GetWindowPlacement が失敗して保存できない
        // -------------------------------------------------------------------------------
        SaveWindowPlacement();
        break;

    default:
        break;
    }
}

// -------------------------------------------------------------------------------
// 予約されたサイズ変更の反映
// -------------------------------------------------------------------------------
void Application::ApplyPendingResize()
{
    if (!m_ResizePending)
    {
        return;
    }

    m_ResizePending = false;

    // 同じ大きさなら何もしない
    if (m_PendingWidth == m_Width && m_PendingHeight == m_Height)
    {
        return;
    }

    // バックバッファ・深度バッファ・ビューポートをまとめて作り直す
    if (!m_GraphicsView.Resize(m_PendingWidth, m_PendingHeight))
    {
        ELOG("Application::ApplyPendingResize() GraphicsView::Resize failed");
        return;
    }

    m_Width  = m_PendingWidth;
    m_Height = m_PendingHeight;

    // -------------------------------------------------------------------------------
    // UI側の画面サイズもここで更新する
    //
    // ドッキング領域はこの矩形から計算されるため、
    // 更新しないとパネルが古い大きさのまま取り残される
    // -------------------------------------------------------------------------------
    m_EditorUIContext.UpdateDockSpaceLayout(
        EditorUI::MakeRect(
            { 0.0f, 0.0f },
            { static_cast<float>(m_Width), static_cast<float>(m_Height) }));
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

    case WM_SETCURSOR:
        // -------------------------------------------------------------------------------
        // クライアント領域のカーソルは自分で決めたので、DefWindowProcへ渡さない
        //
        // 渡すとウィンドウクラスの既定カーソル(矢印)で上書きされ、
        // 入力欄のIビームなどが一瞬で戻ってしまう
        // -------------------------------------------------------------------------------
        if (pApp != nullptr && LOWORD(_lp) == HTCLIENT)
        {
            return TRUE;
        }
        break;

    default:
        break;
    }

    return DefWindowProc(_hWnd, _msg, _wp, _lp);
}
