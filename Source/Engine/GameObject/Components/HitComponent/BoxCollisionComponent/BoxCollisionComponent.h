#pragma once
#include <Engine/GameObject/Component/Component.h>
#include <Engine/GameObject/Components/HitComponent/BoxCollisionComponent/BoxCollision/BoxCollision.h>
#include <Engine/GameObject/Renderable/IDebugLineRenderable.h>

class DebugLineRenderQueue;

// -------------------------------------------------------------------------------
// BoxCollisionComponent class
// 
// 概要 : 
//	Box型の、AABB(Axis-Aligned Bounding Box)の当たり判定を持つコンポーネント
// -------------------------------------------------------------------------------
class BoxCollisionComponent : public Component , public IDebugLineRenderable
{
public:
	// -------------------------------------------------------------------------------
	// コンストラクタ
	// -------------------------------------------------------------------------------
	BoxCollisionComponent();
	// -------------------------------------------------------------------------------
	// デストラクタ
	// -------------------------------------------------------------------------------
	~BoxCollisionComponent() = default;

	// -------------------------------------------------------------------------------
	// Componentインターフェースの実装
	// -------------------------------------------------------------------------------

	// @brief	毎フレームの更新
	void Update(float _deltaTime) override;

	// -------------------------------------------------------------------------------
	// DebugLine描画要求
	// -------------------------------------------------------------------------------
	void SubmitDebugLine(DebugLineRenderQueue* _pQueue) const override;

	// -------------------------------------------------------------------------------
	// 当たり判定
	// -------------------------------------------------------------------------------

	// @brief	ワールド座標上のAABBを返す
	const AABB& GetWorldAABB() const;

	// @brief	サイズの設定
	void SetSize(Math::Vector3 _halfExtents);

	// @brief	座標のオフセット
	void SetOffset(Math::Vector3 _offsetPosition);

	// @brief	他のColliderとヒットしているか
	bool IsHit(const BoxCollisionComponent& _other) const;

	// -------------------------------------------------------------------------------
	// Debug描画ON/OFF
	// -------------------------------------------------------------------------------
	void SetDebugDrawEnabled(bool _enabled);
	bool IsDebugDrawEnabled() const;

	// -------------------------------------------------------------------------------
	// Debug描画色
	// -------------------------------------------------------------------------------
	void SetDebugColor(const Math::Vector4& _color);

	// -------------------------------------------------------------------------------
	// ローカル座標上のAABBを返す
	// -------------------------------------------------------------------------------
	const AABB& GetLocalAABB() const;

	// -------------------------------------------------------------------------------
	// Debug描画色取得
	// -------------------------------------------------------------------------------
	const Math::Vector4& GetDebugColor() const;

private:

	AABB m_LocalAABB;	// ローカル座標上のAABB
	AABB m_WorldAABB;	// ワールド座標上のAABB

	bool m_IsDebugDrawEnabled = true;	// デバッグ描画するか

	Math::Vector4	m_DebugColor
	{
		0.0f,
		1.0f,
		0.0f,
		1.0f
	};

};
