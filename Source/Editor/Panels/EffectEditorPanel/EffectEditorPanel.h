#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Editor/Panels/IEditorPanel.h>
#include <Editor/Effect/EffectAsset.h>
#include <Editor/Effect/EffectPreview.h>

namespace Editor
{
	// -------------------------------------------------------------------------------
	// EffectEditorPanel class
	//
	// 概要 :
	//	1つのエフェクトを編集する専用ウィンドウ（Niagaraのエディタに相当）
	//
	//	他のパネルと違い、アセット1つにつき1枚が動的に開かれる
	//	そのためタイトルにファイル名を含め、IsTransientをtrueにして
	//	閉じたらインスタンスごと破棄されるようにしている
	//
	//	画面構成
	//		上段 : 再生 / 一時停止 / 最初から、保存
	//		中段 : プレビュー（EffectPreviewが動かした粒をその場に描く）
	//		下段 : パラメータ（発生・動き・見た目・再生）
	//
	//	プレビューはこのウィンドウの中だけで完結するため、
	//	3Dシーンやレンダーターゲットには一切影響しない
	// -------------------------------------------------------------------------------
	class EffectEditorPanel : public IEditorPanel
	{
	public:

		// -------------------------------------------------------------------------------
		// @brief	.effectファイルに対応するエディタを開く
		//
		//	すでに同じファイルのエディタが開いていれば、それを手前に出すだけにする
		//	同じアセットのウィンドウが二重に開くと、どちらの編集が正か分からなくなるため
		//
		// @param[in]	_ctx	パネルの登録先を含む共有参照
		// @param[in]	_path	開く.effectファイル
		// @return	開かれた（またはすでに開いていた）パネル
		// -------------------------------------------------------------------------------
		static EffectEditorPanel* OpenForAsset(EditorContext& _ctx, const std::filesystem::path& _path);

		// -------------------------------------------------------------------------------
		// @brief	ファイルを持たない、その場かぎりのエディタを開く
		//
		//	メニューの「新規エフェクトエディタ」から使う
		// -------------------------------------------------------------------------------
		static EffectEditorPanel* OpenScratch(EditorContext& _ctx);

		explicit EffectEditorPanel(std::string _title, std::filesystem::path _path);

		const std::string& GetTitle() const override { return m_Title; }

		void OnGUI(EditorContext& _ctx) override;

		// 閉じたら破棄する。開き直すときは必ずファイルから読み直される
		bool IsTransient() const override { return true; }

	private:

		// -------------------------------------------------------------------------------
		// 画面の各段
		// -------------------------------------------------------------------------------
		void DrawToolbar(EditorContext& _ctx);
		void DrawPreview(EditorContext& _ctx);
		void DrawParameters(EditorContext& _ctx);

		// ファイルへの読み書き。パスが空のときは何もしない
		bool LoadFromFile();
		bool SaveToFile();

		std::string				m_Title;
		std::filesystem::path	m_Path;		// 空ならファイルに紐づかない一時的な編集

		EffectAsset		m_Effect;
		EffectPreview	m_Preview;

		// 未保存の変更があるか。タイトル横に印を出して気づけるようにする
		bool m_Dirty = false;

		// プレビュー領域の高さ。パラメータが増えても見やすい大きさを保つ
		float m_PreviewHeight = 200.0f;
	};
}
