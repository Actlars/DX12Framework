#include "InputManager.h"

bool Input::InputManager::Init(HWND _hWnd)
{
    if (_hWnd == nullptr) { return false; }

    m_hWnd = _hWnd;

    m_KeyboardInput.Reset();
    m_MouseInput.Reset();

    m_MouseCaptureOwner = MouseCaptureOwner::None;

    return true;
}

void Input::InputManager::Term()
{
    ReleaseMouseCapture();
    m_KeyboardInput.Reset();
    m_MouseInput.Reset();

    m_hWnd = nullptr;
}

void Input::InputManager::Update()
{
    if (m_hWnd == nullptr) 
    { return; }

    const bool isWindowActive = GetForegroundWindow() == m_hWnd;

    // フォーカスが外れたらカメラ操作を終了
    if (!isWindowActive) 
    { ReleaseMouseCapture(); }

    m_KeyboardInput.Update(isWindowActive);
    m_MouseInput.Update(m_hWnd, isWindowActive);
}

void Input::InputManager::ResolveMouseCapture(bool _mouseOverEditorUI)
{
    if (m_hWnd == nullptr) 
    { return; }

    // 左クリックした瞬間に入力先を決定
    if (m_MouseInput.IsPressed(MouseButton::Left) && m_MouseCaptureOwner == MouseCaptureOwner::None)
    {
        if (_mouseOverEditorUI)
        { 
            // 前フレームのカメラ操作状態が残っていても、UI操作を最優先に戻す
            if (m_MouseInput.IsRelativeMode())
            {
                m_MouseInput.SetRelativeMode(m_hWnd, false);
            }

            m_MouseCaptureOwner = MouseCaptureOwner::EditorUI; 
        }
        else if(m_MouseCaptureOwner == MouseCaptureOwner::None)
        {
            m_MouseCaptureOwner = MouseCaptureOwner::SceneCamera;
            m_MouseInput.SetRelativeMode(m_hWnd, true);
        }
    }

    // 左ボタンを離したら操作権を解放
    if (m_MouseInput.IsReleased(MouseButton::Left))
    { ReleaseMouseCapture(); }
}

// -------------------------------------------------------------------------------
// ポーリングでは取得できない入力をウィンドウメッセージから受け取る
//
//	ホイールの回転量	: 状態ではなく増分なので、メッセージでしか取れない
//	確定文字(WM_CHAR)	: IMEやキーボードレイアウトを解決した後の文字
//	キーの押下(WM_KEYDOWN) : 長押し時のオートリピートを取りこぼさないため
// -------------------------------------------------------------------------------
void Input::InputManager::ProcessWindowMessage(UINT _msg, WPARAM _wp, LPARAM _lp)
{
    switch (_msg)
    {
    case WM_MOUSEWHEEL:
    {
        // WHEEL_DELTAで割ることで「ホイール1刻み = 1.0」に正規化する
        const float wheelDelta =
            static_cast<float>(GET_WHEEL_DELTA_WPARAM(_wp)) /
            static_cast<float>(WHEEL_DELTA);

        m_MouseInput.AddWheelDelta(wheelDelta);
        break;
    }

    case WM_CHAR:
        m_KeyboardInput.AddInputCharacter(static_cast<wchar_t>(_wp));
        break;

    // Altとの同時押しはWM_SYSKEYDOWNで届くため、両方を同じ扱いにする
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        m_KeyboardInput.AddKeyDownEvent(static_cast<uint32_t>(_wp));
        break;

    default:
        break;
    }
}

void Input::InputManager::ReleaseMouseCapture()
{
    if (m_hWnd != nullptr && m_MouseInput.IsRelativeMode())
    { m_MouseInput.SetRelativeMode(m_hWnd, false); }

    m_MouseCaptureOwner = MouseCaptureOwner::None;
}
