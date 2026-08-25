// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "EffectPreview.h"
#include <Engine/EditorUI/Core/DrawList/DrawList.h>

namespace
{
	constexpr float kPi = 3.14159265358979323846f;

	// 1フレームの経過時間が極端に大きいとき（デバッガで止めた直後など）
	// パーティクルが一気に飛ぶのを防ぐための上限
	constexpr float kMaxDeltaTime = 1.0f / 20.0f;
}

// -------------------------------------------------------------------------------
// 1フレーム進める
//
// 処理の順番
//	1. 発生数を決めて新しい粒を出す
//	2. 生きている粒を動かし、寿命を進める
// -------------------------------------------------------------------------------
void Editor::EffectPreview::Update(
	const EffectAsset&			_effect,
	float						_deltaTime,
	const DirectX::XMFLOAT2&	_origin)
{
	// 粒の入れ物は最大数に合わせて用意する
	const std::size_t capacity = static_cast<std::size_t>((std::max)(1, _effect.MaxParticles));
	if (m_Particles.size() != capacity)
	{
		m_Particles.assign(capacity, Particle{});
		m_NextSlot = 0;
	}

	const float deltaTime = (std::min)((std::max)(_deltaTime, 0.0f), kMaxDeltaTime);

	if (m_Playing)
	{
		m_PlayTime += deltaTime;

		// ループしない設定では、再生時間を過ぎたら発生を止める
		// すでに出ている粒はそのまま寿命まで動かす
		const bool spawning = _effect.Looping || (m_PlayTime <= _effect.Duration);

		if (spawning && _effect.SpawnRate > 0.0f)
		{
			// 「1フレームに何粒出すか」は小数になるため、端数を持ち越して数を合わせる
			m_SpawnAccumulator += _effect.SpawnRate * deltaTime;

			while (m_SpawnAccumulator >= 1.0f)
			{
				m_SpawnAccumulator -= 1.0f;
				Emit(_effect, _origin);
			}
		}
	}

	// -------------------------------------------------------------------------------
	// 生きている粒の更新
	// -------------------------------------------------------------------------------
	m_LiveCount = 0;

	for (Particle& particle : m_Particles)
	{
		if (particle.Life <= 0.0f)
		{
			continue;	// 未使用スロット
		}

		if (m_Playing)
		{
			particle.Age += deltaTime;

			if (particle.Age >= particle.Life)
			{
				particle.Life = 0.0f;	// 寿命切れ。スロットを解放する
				continue;
			}

			// 減衰は「1秒でDragの割合だけ速度を失う」という素直な近似にしている
			const float damping = (std::max)(0.0f, 1.0f - _effect.Drag * deltaTime);

			particle.Velocity.x *= damping;
			particle.Velocity.y = particle.Velocity.y * damping + _effect.Gravity * deltaTime;

			particle.Position.x += particle.Velocity.x * deltaTime;
			particle.Position.y += particle.Velocity.y * deltaTime;
		}

		++m_LiveCount;
	}
}

// -------------------------------------------------------------------------------
// 現在のパーティクルを描く
//
// 粒は正方形で描く。プレビューの目的は「動きと色の確認」なので、
// 形よりも数と負荷の軽さを優先している
// -------------------------------------------------------------------------------
void Editor::EffectPreview::Draw(
	const EffectAsset&		_effect,
	EditorUI::DrawList&		_drawList,
	const EditorUI::Rect2D&	_clipRect) const
{
	// 色は毎フレーム同じ変換になるため、粒ごとではなくここで1回だけ求める
	const EditorUI::Color32 startColor	= EffectAsset::ToColor32(_effect.StartColor);
	const EditorUI::Color32 endColor	= EffectAsset::ToColor32(_effect.EndColor);

	// プレビュー領域の外へ飛んだ粒を描かないようにする
	_drawList.PushClipRect(_clipRect);

	for (const Particle& particle : m_Particles)
	{
		if (particle.Life <= 0.0f)
		{
			continue;
		}

		// 寿命に対する進み具合(0.0=生成直後, 1.0=消滅直前)
		const float t = std::clamp(particle.Age / particle.Life, 0.0f, 1.0f);

		// 大きさと色を、開始値から終了値へ線形に補間する
		const float size = _effect.StartSize + (_effect.EndSize - _effect.StartSize) * t;
		const float half = (std::max)(0.5f, size * 0.5f);

		const EditorUI::Color32 color = EditorUI::LerpColor(startColor, endColor, t);

		_drawList.AddRectFilled(
			{
				{ particle.Position.x - half, particle.Position.y - half },
				{ particle.Position.x + half, particle.Position.y + half }
			},
			color);
	}

	_drawList.PopClipRect();
}

void Editor::EffectPreview::Restart()
{
	for (Particle& particle : m_Particles)
	{
		particle.Life = 0.0f;
	}

	m_SpawnAccumulator	= 0.0f;
	m_PlayTime			= 0.0f;
	m_LiveCount			= 0;
	m_Playing			= true;
}

// -------------------------------------------------------------------------------
// 粒を1つ出す
//
// 空きスロットは前回の続きから探す
// 毎回先頭から探すと、粒が多いときに走査が無駄に長くなるため
// -------------------------------------------------------------------------------
void Editor::EffectPreview::Emit(const EffectAsset& _effect, const DirectX::XMFLOAT2& _origin)
{
	const std::size_t count = m_Particles.size();
	if (count == 0)
	{ return; }

	std::size_t slot = count;
	for (std::size_t i = 0; i < count; ++i)
	{
		const std::size_t index = (m_NextSlot + i) % count;
		if (m_Particles[index].Life <= 0.0f)
		{
			slot = index;
			break;
		}
	}

	if (slot == count)
	{
		return;	// 全部埋まっている。上限に達しているので今回は出さない
	}

	m_NextSlot = (slot + 1) % count;

	Particle& particle = m_Particles[slot];

	// -------------------------------------------------------------------------------
	// 発生位置
	// -------------------------------------------------------------------------------
	DirectX::XMFLOAT2 position = _origin;

	switch (_effect.Shape)
	{
	case EmitterShape::Circle:
	{
		const float angle = Random01() * kPi * 2.0f;
		position.x += std::cos(angle) * _effect.ShapeRadius;
		position.y += std::sin(angle) * _effect.ShapeRadius;
		break;
	}
	case EmitterShape::Box:
	{
		position.x += (Random01() * 2.0f - 1.0f) * _effect.ShapeRadius;
		position.y += (Random01() * 2.0f - 1.0f) * _effect.ShapeRadius;
		break;
	}
	case EmitterShape::Point:
	default:
		break;
	}

	// -------------------------------------------------------------------------------
	// 初速
	// SpreadAngleは「真上を中心に、左右へどれだけ広がるか」として扱う
	// -------------------------------------------------------------------------------
	const float spread	= _effect.SpreadAngle * (kPi / 180.0f);
	const float angle	= -kPi * 0.5f + (Random01() - 0.5f) * spread;

	const float speed = _effect.InitialSpeed +
		(Random01() * 2.0f - 1.0f) * _effect.InitialSpeedRandom;

	particle.Position	= position;
	particle.Velocity	= { std::cos(angle) * speed, std::sin(angle) * speed };
	particle.Age		= 0.0f;

	// 寿命が0以下になると「未使用」と区別できなくなるため、必ず正の値にする
	particle.Life = (std::max)(
		0.01f,
		_effect.LifeTime + (Random01() * 2.0f - 1.0f) * _effect.LifeTimeRandom);
}

// -------------------------------------------------------------------------------
// 0.0～1.0の乱数
//
// 見た目のばらつきが目的なので、品質より速度と再現性を優先した簡易な生成方法にしている
// -------------------------------------------------------------------------------
float Editor::EffectPreview::Random01()
{
	// xorshift32
	m_RandomState ^= m_RandomState << 13;
	m_RandomState ^= m_RandomState >> 17;
	m_RandomState ^= m_RandomState << 5;

	return static_cast<float>(m_RandomState & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}
