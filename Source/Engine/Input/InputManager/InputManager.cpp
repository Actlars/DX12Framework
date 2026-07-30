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
        { m_MouseCaptureOwner = MouseCaptureOwner::EditorUI; }
        else
        {
            m_MouseCaptureOwner = MouseCaptureOwner::SceneCamera;
            m_MouseInput.SetRelativeMode(m_hWnd, true);
        }
    }

    // 左ボタンを離したら操作権を解放
    if (m_MouseInput.IsReleased(MouseButton::Left))
    { ReleaseMouseCapture(); }
}

void Input::InputManager::ReleaseMouseCapture()
{
    if (m_hWnd != nullptr && m_MouseInput.IsRelativeMode())
    { m_MouseInput.SetRelativeMode(m_hWnd, false); }

    m_MouseCaptureOwner = MouseCaptureOwner::None;
}
