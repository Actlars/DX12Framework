#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Editor/EditorContext.h>

namespace Editor
{
	// -------------------------------------------------------------------------------
	// IEditorPanel インターフェース
	//
	// 概要 :
	//	エディタのウィンドウ1枚が満たすべき最小の約束
	//
	//	派生クラスは「中身をどう描くか」(OnGUI)だけを書けばよい
	//	ウィンドウの開始/終了・閉じるボタン・ドッキングはPanelManagerが受け持つため、
	//	パネル側にBeginWindow / EndWindowは一切現れない
	//
	//	この分離により、新しいパネルを足す作業は
	//	「1クラス書いてPanelManagerへ登録する」だけになる
	// -------------------------------------------------------------------------------
	class IEditorPanel
	{
	public:

		virtual ~IEditorPanel() = default;

		// -------------------------------------------------------------------------------
		// @brief	ウィンドウのタイトル
		//
		//	表示名であると同時に、EditorUIがウィンドウを識別するIdの元にもなる
		//	そのため、同時に存在するパネル同士で必ず異なる文字列にする
		// -------------------------------------------------------------------------------
		virtual const std::string& GetTitle() const = 0;

		// -------------------------------------------------------------------------------
		// @brief	ウィンドウの中身を積む
		//
		//	BeginWindowが成功したときだけ呼ばれる
		//	折り畳み中や、非アクティブなタブのときは呼ばれない
		// -------------------------------------------------------------------------------
		virtual void OnGUI(EditorContext& _ctx) = 0;

		// -------------------------------------------------------------------------------
		// @brief	ウィンドウの見た目の指定
		//
		//	ゲームビューのようにスクロールが不要なパネルが上書きする
		// -------------------------------------------------------------------------------
		virtual EditorUI::WindowFlags GetWindowFlags() const { return EditorUI::WindowFlags::None; }

		// -------------------------------------------------------------------------------
		// @brief	閉じられるパネルか
		//
		//	falseを返すとタイトルバーに×が出ず、常に表示され続ける
		// -------------------------------------------------------------------------------
		virtual bool IsClosable() const { return true; }

		// -------------------------------------------------------------------------------
		// @brief	閉じられたあと、インスタンスごと破棄してよいか
		//
		//	常設パネル(ヒエラルキー等)はfalseで、閉じても状態を残す
		//	エフェクトエディタのように動的に開くものはtrueにして、閉じたら消す
		// -------------------------------------------------------------------------------
		virtual bool IsTransient() const { return false; }

		// -------------------------------------------------------------------------------
		// 表示状態
		//	EditorUIのBeginWindowへbool*として渡すため、参照を返せるようにしている
		// -------------------------------------------------------------------------------
		bool& GetOpenFlag()			{ return m_Open; }
		bool  IsOpen()		const	{ return m_Open; }
		void  SetOpen(bool _open)	{ m_Open = _open; }

		// -------------------------------------------------------------------------------
		// 初回に開いたときの位置とサイズ
		//	ドッキング前のフローティング表示で使われる
		// -------------------------------------------------------------------------------
		const DirectX::XMFLOAT2& GetInitialPosition() const { return m_InitialPosition; }
		const DirectX::XMFLOAT2& GetInitialSize()	  const { return m_InitialSize; }

		void SetInitialPlacement(const DirectX::XMFLOAT2& _position, const DirectX::XMFLOAT2& _size)
		{
			m_InitialPosition	= _position;
			m_InitialSize		= _size;
		}

		// EditorUI側のウィンドウId。PanelManagerが破棄要求を出すために保持する
		EditorUI::Id GetWindowId() const		{ return m_WindowId; }
		void SetWindowId(EditorUI::Id _id)		{ m_WindowId = _id; }

	protected:

		bool				m_Open = true;
		EditorUI::Id		m_WindowId = 0;

		DirectX::XMFLOAT2	m_InitialPosition{ 80.0f, 80.0f };
		DirectX::XMFLOAT2	m_InitialSize	 { 360.0f, 420.0f };
	};
}
