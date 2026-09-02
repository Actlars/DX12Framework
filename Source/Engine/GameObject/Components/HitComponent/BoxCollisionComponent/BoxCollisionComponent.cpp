#include "BoxCollisionComponent.h"
#include <Engine/GameObject/GameObject.h>
#include <Engine/GameObject/Components/TransformComponent/TransformComponent.h>
#include <Engine/Renderer/RenderQueue/DebugLineRenderQueue/DebugLineRenderQueue.h>

BoxCollisionComponent::BoxCollisionComponent()
{/* DO_NOTHING */ }

void BoxCollisionComponent::Update(float _deltaTime)
{
	const Math::Vector3 transform = m_pOwner->GetComponent<TransformComponent>()->GetPosition();

	m_WorldAABB.Center		= transform + m_LocalAABB.Center;
	m_WorldAABB.HalfExtents = m_LocalAABB.HalfExtents;
}

void BoxCollisionComponent::SubmitDebugLine(DebugLineRenderQueue* _pQueue) const
{
	if (_pQueue == nullptr || !m_IsDebugDrawEnabled)
	{
		return;
	}

	_pQueue->SubmitAABB(m_WorldAABB.Center, m_WorldAABB.HalfExtents, m_DebugColor);
}

const AABB& BoxCollisionComponent::GetWorldAABB() const
{
	return m_WorldAABB;
}

void BoxCollisionComponent::SetSize(Math::Vector3 _halfExtents)
{
	m_LocalAABB.HalfExtents = _halfExtents;
}

void BoxCollisionComponent::SetOffset(Math::Vector3 _offsetPosition)
{
	m_LocalAABB.Center = _offsetPosition;
}

bool BoxCollisionComponent::IsHit(const BoxCollisionComponent& _other) const
{
	return ::IsHit(m_WorldAABB, _other.m_WorldAABB);
}

void BoxCollisionComponent::SetDebugDrawEnabled(bool _enabled)
{
	m_IsDebugDrawEnabled = _enabled;
}

bool BoxCollisionComponent::IsDebugDrawEnabled() const
{
	return m_IsDebugDrawEnabled;
}

void BoxCollisionComponent::SetDebugColor(const Math::Vector4& _color)
{
	m_DebugColor = _color;
}

const AABB& BoxCollisionComponent::GetLocalAABB() const
{
	return m_LocalAABB;
}

const Math::Vector4& BoxCollisionComponent::GetDebugColor() const
{
	return m_DebugColor;
}
