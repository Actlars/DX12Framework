#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Editor/Effect/EffectAsset.h>

namespace EditorUI { class DrawList; }

namespace Editor
{
	// -------------------------------------------------------------------------------
	// EffectPreview class
	//
	// 概要 :
	//	EffectAssetのパラメータを、その場で動かして見せるためのCPUシミュレーション
	//
	//	Niagaraのプレビューと同じ役割で、
	//	「値をいじった結果がすぐ目に見える」ことだけを目的にしている
	//	そのため描画はEditorUIのDrawListへの矩形の積み上げで済ませ、
	//	GPUパーティクルの仕組みには一切依存しない
	//
	//	責務の分担 :
	//		EffectAsset		どんなエフェクトか（データ）
	//		EffectPreview	いま何粒がどこにいるか（実行時の状態）
	//		EffectEditorPanel	それをどこに、どう表示するか（UI）
	// -------------------------------------------------------------------------------
	class EffectPreview
	{
	public:

		// -------------------------------------------------------------------------------
		// @brief	1フレーム進める
		//
		// @param[in]	_effect		再生するパラメータ
		// @param[in]	_deltaTime	経過時間(秒)
		// @param[in]	_origin		発生位置（プレビュー領域のスクリーン座標）
		// -------------------------------------------------------------------------------
		void Update(const EffectAsset& _effect, float _deltaTime, const DirectX::XMFLOAT2& _origin);

		// -------------------------------------------------------------------------------
		// @brief	現在のパーティクルを描く
		//
		// @param[in]	_effect		大きさと色の補間に使うパラメータ
		// @param[in]	_drawList	描画先
		// @param[in]	_clipRect	この矩形からはみ出した分は描かない
		// -------------------------------------------------------------------------------
		void Draw(
			const EffectAsset&			_effect,
			EditorUI::DrawList&			_drawList,
			const EditorUI::Rect2D&		_clipRect) const;

		// -------------------------------------------------------------------------------
		// 再生制御
		// -------------------------------------------------------------------------------
		void Play()		{ m_Playing = true; }
		void Pause()	{ m_Playing = false; }
		void Restart();					// 粒を全部消して最初から
		bool IsPlaying() const { return m_Playing; }

		int32_t GetLiveCount()	const { return m_LiveCount; }
		float	GetPlayTime()	const { return m_PlayTime; }

	private:

		// -------------------------------------------------------------------------------
		// Particle struct
		//
		// 概要 :
		//	粒1つ分の状態
		//	Ageが寿命に達した時点で「死んだ」とみなし、次の発生で使い回す
		// -------------------------------------------------------------------------------
		struct Particle
		{
			DirectX::XMFLOAT2	Position{};
			DirectX::XMFLOAT2	Velocity{};
			float				Age		= 0.0f;
			float				Life	= 0.0f;	// 0以下なら未使用
		};

		// 空きスロットを1つ探して粒を出す。見つからなければ何もしない
		void Emit(const EffectAsset& _effect, const DirectX::XMFLOAT2& _origin);

		// 0.0～1.0の乱数
		float Random01();

		std::vector<Particle>	m_Particles;
		std::size_t				m_NextSlot	= 0;	// 空きスロット探索の開始位置

		float	m_SpawnAccumulator	= 0.0f;	// 発生数の端数。低いSpawnRateでも正しく間引く
		float	m_PlayTime			= 0.0f;
		bool	m_Playing			= true;
		int32_t	m_LiveCount			= 0;

		// 乱数はプレビューごとに独立させたいのでメンバに持つ
		uint32_t m_RandomState = 0x9E3779B9u;
	};
}
