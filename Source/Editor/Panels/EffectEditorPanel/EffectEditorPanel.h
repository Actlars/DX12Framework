#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Editor/Panels/IEditorPanel.h>
#include <Editor/Effect/EffectAsset.h>
#include <Engine/Effect/GpuParticleSystem/GpuParticleSystem.h>
#include <Engine/Renderer/ViewportTarget/ViewportTarget.h>

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
	// 画面構成 :
	//	上段 : 再生 / 一時停止 / 最初から、保存、粒の数
	//	中段 : プレビュー（GPUパーティクルを専用のレンダーターゲットへ描いたもの）
	//	下段 : パラメータ（発生・動き・見た目・再生）
	//
	// 責務の分担 :
	//	EffectAsset			どんなエフェクトか（データ）
	//	GpuParticleSystem	GPU上での発生・更新・描画（エンジン側の実行基盤）
	//	EffectEditorPanel	値の編集と、プレビューをどこに出すか（このクラス）
	//
	// 描画の流れ :
	//	OnGUI	 プレビューの領域を確保し、前フレームの絵を貼る
	//	OnRender 確保した大きさでレンダーターゲットを用意し、GPUで粒を進めて描く
	//
	//	UIの構築と実際の描画は別のタイミングで行われるため、
	//	表示されるのは常に「1フレーム前の絵」になる
	//	その代わりGPUの完了待ちが一切発生しない
	// -------------------------------------------------------------------------------
	class EffectEditorPanel : public IEditorPanel
	{
	public:

		// -------------------------------------------------------------------------------
		// @brief	.effectファイルに対応するエディタを開く
		//
		//	すでに同じファイルのエディタが開いていれば、それを手前に出すだけにする
		//	同じアセットのウィンドウが二重に開くと、どちらの編集が正か分からなくなるため
		// -------------------------------------------------------------------------------
		static EffectEditorPanel* OpenForAsset(EditorContext& _ctx, const std::filesystem::path& _path);

		// @brief	ファイルを持たない、その場かぎりのエディタを開く
		static EffectEditorPanel* OpenScratch(EditorContext& _ctx);

		explicit EffectEditorPanel(std::string _title, std::filesystem::path _path);
		~EffectEditorPanel() override;

		const std::string& GetTitle() const override { return m_Title; }

		void OnGUI(EditorContext& _ctx) override;
		void OnRender(EditorContext& _ctx, ID3D12GraphicsCommandList* _pCmd) override;

		// 閉じたら破棄する。開き直すときは必ずファイルから読み直される
		bool IsTransient() const override { return true; }

	private:

		// -------------------------------------------------------------------------------
		// 画面の各段
		// -------------------------------------------------------------------------------
		void DrawToolbar(EditorContext& _ctx);
		void DrawPreview(EditorContext& _ctx);
		void DrawParameters(EditorContext& _ctx);

		// プレビュー用のGPUリソースを必要になった時点で確保する
		bool EnsurePreviewResources(EditorContext& _ctx);

		// EffectAssetの値から、このフレームの発生パラメータを組み立てる
		Effect::EmitterParams BuildEmitterParams(float _deltaTime);

		// プレビューカメラの行列と、ビルボード用の基底を組み立てる
		Effect::ParticleViewParams BuildViewParams() const;

		// ファイルへの読み書き。パスが空のときは何もしない
		bool LoadFromFile();
		bool SaveToFile();

		std::string				m_Title;
		std::filesystem::path	m_Path;		// 空ならファイルに紐づかない一時的な編集

		EffectAsset m_Effect;

		// -------------------------------------------------------------------------------
		// プレビュー用のGPUリソース
		//
		//	エディタを開いたときに初めて確保する
		//	パネルを開かないかぎりVRAMを消費しない
		// -------------------------------------------------------------------------------
		std::unique_ptr<ViewportTarget>				m_pPreviewTarget;
		std::unique_ptr<Effect::GpuParticleSystem>	m_pParticles;

		// このフレームにプレビューへ必要な大きさ。0なら描く必要がない
		uint32_t m_PreviewWidth		= 0;
		uint32_t m_PreviewHeight	= 0;

		// 再生状態
		bool	m_Playing	= true;
		float	m_PlayTime	= 0.0f;

		// 発生数の端数。低いSpawnRateでも正しい間隔で出すために持ち越す
		float	m_SpawnAccumulator = 0.0f;

		// 未保存の変更があるか。上段に印を出して気づけるようにする
		bool	m_Dirty = false;

		// プレビューカメラ。マウスのドラッグで回せるようにしている
		float	m_CameraYaw		= 0.6f;
		float	m_CameraPitch	= 0.35f;
		float	m_CameraDistance = 3.5f;

		// プレビュー領域の高さ
		float	m_PreviewHeightRatio = 0.42f;
	};
}
