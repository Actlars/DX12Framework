#pragma once

// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Context/FrameContext/FrameContext.h>
#include <Engine/EditorUI/Core/Context/WidgetInteractionState/WidgetInteractionState.h>
#include <Engine/EditorUI/Core/Context/WindowInteraction/WindowInteraction.h>
#include <Engine/EditorUI/Core/Context/WindowManager/WindowManager.h>
#include <Engine/EditorUI/Core/Id.h>
#include <Engine/EditorUI/Core/InputTracker/InputTracker.h>
#include <Engine/EditorUI/Core/Style.h>
#include <Engine/EditorUI/Core/Window.h>
#include <Engine/EditorUI/Docking/DockController/DockController.h>
#include "FrameContext/FrameOutput.h"

namespace EditorUI
{
	// Fontはテクスチャ(RHI)に依存するため、Coreからは前方宣言だけを見る
	class Font;

	// -------------------------------------------------------------------------------
	// Context class
	//
	// EditorUIの公開Facade
	//
	// 責務 :
	//	各サブシステム(入力・ウィンドウ・ドッキング・ウィジェット状態)を所有し、
	//	それらの「呼び出し順序」だけを決める調停役
	//	個々のアルゴリズムは持たず、必ずサブシステムへ委譲する
	//
	// 1フレームの流れ :
	//	NewFrame()          入力更新 → 遅延削除 → ホバー/フォーカス確定 → ドッキング確定
	//	BeginWindow()/...   アプリがウィンドウとウィジェットを積む
	//	EndFrame()          Z順に従ってDrawListを並べ、描画出力を確定する
	// -------------------------------------------------------------------------------
	class Context
	{
	public:

		Context();
		~Context() = default;

		void NewFrame(const InputState& _input);
		void EndFrame();

		// -------------------------------------------------------------------------------
		// 初期化・共有リソース
		// -------------------------------------------------------------------------------

		void InitDockSpace(const Rect2D& _screenBounds);
		void UpdateDockSpaceLayout(const Rect2D& _screenBounds);

		// 画面全体の矩形。ポップアップの画面内収めや外周ドッキングの基準に使う
		const Rect2D& GetScreenBounds() const;

		// -------------------------------------------------------------------------------
		// @brief	ドッキングに使ってよい領域を指定する
		//
		//	画面上端のメニューバーのように、ドッキング対象にしたくない固定の帯がある場合に、
		//	その分だけ狭めた矩形を渡す
		//	指定しなければ画面全体がそのままドッキング領域になる
		// -------------------------------------------------------------------------------
		void SetDockArea(const Rect2D& _dockArea);

		// -------------------------------------------------------------------------------
		// 既定レイアウトの構築
		//
		//	起動直後に一度だけ、UE5のような初期配置を組むために使う
		//	以降はユーザーの操作が優先されるため、呼ぶ必要はない
		// -------------------------------------------------------------------------------

		// まだ何もドッキングされていないか
		bool IsDockSpaceEmpty() const;

		// 画面全体を指定方向へ分割し、新しい区画のIdを返す
		int SplitDockArea(DockSplitDir _dir, float _ratio);

		// 指定座標を含む区画のIdを返す
		int FindDockLeafAt(const DirectX::XMFLOAT2& _point) const;

		// ウィンドウ名を指定して、その区画へドッキングさせる
		void DockWindow(std::string_view _title, int _leafId);

		// ドッキング領域全体の矩形
		Rect2D GetDockAreaBounds() const;

		// まだ分割していない状態での中央の区画Id
		int GetRootDockLeafId() const;


		// -------------------------------------------------------------------------------
		// @brief	タイトルバーやタブの文字を描くためのフォントを登録する
		//
		//	ウィジェットは呼び出し側からフォントを受け取るが、
		//	タイトルバーやタブはContextが自分で描くため、ここで1つだけ共有する
		// -------------------------------------------------------------------------------
		void SetFont(Font* _pFont);
		Font* GetFont() const;

		// -------------------------------------------------------------------------------
		// ウィンドウ
		// -------------------------------------------------------------------------------

		bool BeginWindow(
			std::string_view _title,
			bool* _isOpen = nullptr,
			WindowFlags _flags = WindowFlags::None);
		void EndWindow();

		void RequestDestroyWindow(Id _windowId);

		// 次に生成されるウィンドウの初期位置とサイズを指定する（初回のみ有効）
		void SetNextWindowPlacement(
			const DirectX::XMFLOAT2& _position,
			const DirectX::XMFLOAT2& _size);

		// 既存のウィンドウにも強制的に位置とサイズを適用する（ポップアップ用）
		void SetNextWindowPlacementForced(
			const DirectX::XMFLOAT2& _position,
			const DirectX::XMFLOAT2& _size);

		// -------------------------------------------------------------------------------
		// ポップアップ / コンテキストメニュー
		//
		//	ポップアップは「常に最前面に出る、枠なしの小さなウィンドウ」として実装する
		//	専用の描画経路を作らず既存のウィンドウ機構に乗せることで、
		//	クリップ・入力占有・Z順の扱いが本体と自動的に一致する
		// -------------------------------------------------------------------------------

		// @brief	指定した名前のポップアップを、現在のマウス位置に開く
		void OpenPopup(std::string_view _idLabel);

		// @brief	位置を明示してポップアップを開く
		void OpenPopupAt(std::string_view _idLabel, const DirectX::XMFLOAT2& _position);

		bool IsPopupOpen(std::string_view _idLabel) const;

		// @brief	開いていればウィンドウを開始する。trueのときだけ中身を積み、必ずEndPopupで閉じる
		bool BeginPopup(std::string_view _idLabel);
		void EndPopup();

		// @brief	いま描いているポップアップを閉じる。メニュー項目を選んだ直後に呼ぶ
		void CloseCurrentPopup();
		void CloseAllPopups();

		// マウスがいずれかのポップアップの上にあるか。背後のウィジェットを止めるために使う
		bool IsAnyPopupHovered() const;

		// -------------------------------------------------------------------------------
		// スタイル・状態の問い合わせ
		// -------------------------------------------------------------------------------

		const Style& GetStyle() const;
		void SetStyle(const Style& _style);

		IdStack& GetIdStack();
		const IdStack& GetIdStack() const;

		WindowFrame* GetCurrentWindow();
		const WindowFrame* GetCurrentWindow() const;

		// ウィンドウに属さない、常に最前面へ描かれる描画リスト
		DrawList& GetOverlayDrawList();

		bool IsMouseOverAnyWindow() const;
		Id GetHoveredWindow() const;
		bool IsCurrentWindowHovered() const;

		const FrameOutput& GetFrameOutput() const;

		const DirectX::XMFLOAT2& GetMousePos() const;
		DirectX::XMFLOAT2 GetMouseDelta() const;
		float GetMouseWheel() const;
		bool IsMouseDown(MouseButton _button) const;
		bool IsMouseClicked(MouseButton _button) const;
		bool IsMouseReleased(MouseButton _button) const;
		bool IsMouseDoubleClicked(MouseButton _button) const;
		bool IsKeyDown(Key _key) const;
		bool IsKeyPressed(Key _key) const;
		const std::wstring& GetInputCharacters() const;

		// 前フレームからの経過時間(秒)と、起動からの累積時間(秒)
		float  GetDeltaTime() const;
		double GetTime() const;

		// -------------------------------------------------------------------------------
		// ウィジェット用の永続ストレージ
		//
		//	即時モードのウィジェットは自分で状態を持てないが、
		//	ツリーの開閉のように「見た目だけの状態」までアプリに持たせるのは煩雑になる
		//	そうした状態だけをIdをキーにしてここへ預ける
		// -------------------------------------------------------------------------------
		bool GetStorageBool(Id _id, bool _defaultValue) const;
		void SetStorageBool(Id _id, bool _value);

		Id GetActiveId() const;
		bool IsActiveId(Id _id) const;
		bool IsAnyItemActive() const;
		void SetActive(Id _id);
		void ClearActiveId(Id _id);
		void KeepActiveIdAlive(Id _id);

		Id GetHoveredId() const;
		bool IsHoveredId(Id _id) const;
		void SetHoveredId(Id _id);

		const DirectX::XMFLOAT2& GetActiveIdClickPos() const;
		double& GetActiveDragAccumulator();
		TextEditState& GetTextEditState();
		const TextEditState& GetTextEditState() const;

		bool WantCaptureMouse() const;
		bool WantCaptureKeyboard() const;

	private:

		// -------------------------------------------------------------------------------
		// NextWindowPlacement struct
		//
		// 概要 :
		//	「次に開くウィンドウ1つ」に対する配置指定
		//	Forcedがtrueなら既存ウィンドウにも毎フレーム適用する（ポップアップ用）
		// -------------------------------------------------------------------------------
		struct NextWindowPlacement
		{
			bool Pending = false;
			bool Forced  = false;
			DirectX::XMFLOAT2 Position{};
			DirectX::XMFLOAT2 Size{};
		};

		// -------------------------------------------------------------------------------
		// PopupState struct
		//
		// 概要 :
		//	開いているポップアップ1つ分の状態
		//	サイズは中身から毎フレーム測り直すため、ここに前フレームの結果を持つ
		// -------------------------------------------------------------------------------
		struct PopupState
		{
			Id					PopupId = 0;
			DirectX::XMFLOAT2	Anchor{};				// 開いた位置（左上）
			DirectX::XMFLOAT2	Size{ 160.0f, 24.0f };	// 前フレームの中身から求めたサイズ
			Rect2D				Rect{};					// 実際に描かれた矩形（範囲外クリック判定用）
			Id					WindowId = 0;			// 対応するウィンドウのId
		};

		void ProcessPendingDestroyWindows();
		bool TryGetWindowRect(Id _windowId, Rect2D& _outRect) const;
		Id FindTopmostWindowAt(const DirectX::XMFLOAT2& _point) const;
		void HandleWindowFocusClick();
		void ApplyDockTabResult(const DockTabInteractionResult& _result);

		void SetupWindowGeometry(
			WindowFrame& _frame,
			const WindowState& _state,
			WindowFlags _flags,
			const DockWindowInfo& _dockInfo);

		// タイトルバーの文字と×ボタンを描き、閉じる要求が出たらStateへ立てる
		void DrawWindowChrome(
			WindowFrame&	_frame,
			WindowState&	_state,
			const Rect2D&	_titleBarRect,
			bool			_hasCloseButton);

		void FinalizeScrollRange(WindowFrame& _frame, WindowState& _state);

		// 範囲外クリックでポップアップを閉じる
		void UpdatePopupDismiss();

		PopupState* FindPopup(Id _popupId);
		const PopupState* FindPopup(Id _popupId) const;

		Style			m_Style;
		IdStack			m_IdStack;
		InputTracker	m_InputTracker;

		FrameContext			m_FrameContext;
		WindowManager			m_WindowManager;
		WindowInteraction		m_WindowInteraction;
		DockController			m_DockController;
		WidgetInteractionState	m_WidgetInteractionState;

		NextWindowPlacement m_NextWindow;

		// ウィジェットが預けたbool状態(ツリーの開閉など)
		std::unordered_map<Id, bool> m_StorageBool;

		Font*	m_pFont = nullptr;	// 所有権なし。タイトル・タブの文字描画に使う
		Rect2D	m_ScreenBounds{};

		// 開いているポップアップ。末尾が最も手前（ネストしたメニューの最深部）
		std::vector<PopupState> m_OpenPopups;

		// このフレームでポップアップとして描かれたウィンドウ。EndFrameで最後に積む
		std::vector<Id> m_PopupDrawOrder;

		// BeginPopup～EndPopupの入れ子の深さ。EndPopupがどのPopupStateへ書き戻すかに使う
		std::vector<Id> m_PopupStack;
	};
}
