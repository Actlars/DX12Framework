#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Editor/Panels/IEditorPanel.h>

namespace Editor
{
	// -------------------------------------------------------------------------------
	// ViewportPanel class
	//
	// 概要 :
	//	ゲーム画面をエディタの1パネルとして表示するパネル
	//
	//	ゲーム画面はViewportTargetによってテクスチャへ描かれているため、
	//	このパネルがやることは「その1枚をパネルいっぱいに貼る」ことだけになる
	//	結果として、ゲーム画面も他のパネルと同じようにドッキング・リサイズできる
	//
	//	描画されるサイズの要求もここが出す
	//	OnGUIで測った表示領域を記録しておき、EditorAppがそれを見て
	//	レンダーターゲットを作り直す
	//
	//	なお、UIの構築はレンダリングより前に行われるため、
	//	表示されるのは「このフレームに要求したサイズで描かれた1フレーム前の絵」になる
	//	リサイズ中の一瞬だけ引き伸ばされて見えるが、その代わり待ちが発生しない
	// -------------------------------------------------------------------------------
	class ViewportPanel : public IEditorPanel
	{
	public:

		ViewportPanel();

		const std::string& GetTitle() const override { return m_Title; }

		void OnGUI(EditorContext& _ctx) override;

		// ゲーム画面はスクロールもリサイズグリップも不要なので切っておく
		EditorUI::WindowFlags GetWindowFlags() const override
		{
			return EditorUI::WindowFlags::NoScrollbar;
		}

		// -------------------------------------------------------------------------------
		// @brief	このフレームに必要だった描画サイズ
		//
		//	EditorAppがこれを見てViewportTargetを作り直す
		//	どちらかが0なら、表示されていない（描く必要がない）ことを表す
		// -------------------------------------------------------------------------------
		uint32_t GetRequestedWidth()  const { return m_RequestedWidth; }
		uint32_t GetRequestedHeight() const { return m_RequestedHeight; }

		// パネルが画面に出ているか。出ていなければシーンの描画自体を省ける
		bool IsVisibleThisFrame() const { return m_VisibleThisFrame; }

		// マウスがゲーム画面の内側にあるか。カメラ操作を許すかの判断に使う
		bool IsViewportHovered() const { return m_ViewportHovered; }

		// -------------------------------------------------------------------------------
		// @brief	フレームの先頭で、前フレームの結果を初期化する
		//
		//	パネルが閉じられた場合にOnGUIが呼ばれないため、
		//	「描く必要がない」ことを正しく伝えるにはここでの初期化が要る
		// -------------------------------------------------------------------------------
		void BeginFrame();

	private:

		std::string m_Title = "Viewport";

		uint32_t m_RequestedWidth	= 0;
		uint32_t m_RequestedHeight	= 0;

		bool m_VisibleThisFrame	= false;
		bool m_ViewportHovered	= false;
	};
}
