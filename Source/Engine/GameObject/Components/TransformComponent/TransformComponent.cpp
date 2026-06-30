// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "TransformComponent.h"

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
TransformComponent::TransformComponent()
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
TransformComponent::~TransformComponent()
{ /* DO_NOTHING*/ }

// -------------------------------------------------------------------------------
//		更新
// -------------------------------------------------------------------------------
void TransformComponent::Update(float _deltaTime)
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		セッター
// -------------------------------------------------------------------------------
void TransformComponent::SetPosition(const DirectX::XMFLOAT3& _pos) 
{ m_Position = _pos; m_Dirty = true; }
void TransformComponent::SetRotation(const DirectX::XMFLOAT3& _rot) 
{ m_Rotation = _rot; m_Dirty = true; }
void TransformComponent::SetScale(const DirectX::XMFLOAT3& _scale) 
{ m_Scale = _scale; m_Dirty = true; }

// -------------------------------------------------------------------------------
//		ゲッター
// -------------------------------------------------------------------------------
const DirectX::XMFLOAT3& TransformComponent::GetPosition() const 
{ return m_Position; }
const DirectX::XMFLOAT3& TransformComponent::GetRotation() const 
{ return m_Rotation; }
const DirectX::XMFLOAT3& TransformComponent::GetScale() const 
{ return m_Scale; }

// -------------------------------------------------------------------------------
//		位置を加算する
// -------------------------------------------------------------------------------
void TransformComponent::AddPosition(const DirectX::XMFLOAT3& _delta)
{
	m_Position.x += _delta.x;
	m_Position.y += _delta.y;
	m_Position.z += _delta.z;
	m_Dirty = true;
}

// -------------------------------------------------------------------------------
//		回転を加算する
// -------------------------------------------------------------------------------
void TransformComponent::AddRotation(const DirectX::XMFLOAT3& _delta)
{
	m_Rotation.x += _delta.x;
	m_Rotation.y += _delta.y;
	m_Rotation.z += _delta.z;
	m_Dirty = true;
}

// -------------------------------------------------------------------------------
//		ワールド変換行列を返す
// -------------------------------------------------------------------------------
DirectX::XMMATRIX TransformComponent::GetWorldMatrix() const
{
	if (!m_Dirty) 
	{ return m_WorldMatrix; }

	// Scale → Rotation → Translation の順で合成
	const auto S = DirectX::XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);

	const auto R = DirectX::XMMatrixRotationRollPitchYaw(
		DirectX::XMConvertToRadians(m_Rotation.x),
		DirectX::XMConvertToRadians(m_Rotation.y),
		DirectX::XMConvertToRadians(m_Rotation.z));

	const auto T = DirectX::XMMatrixTranslation(
		m_Position.x, m_Position.y, m_Position.z);

	m_WorldMatrix = S * R * T;
	m_Dirty = false;

	return m_WorldMatrix;
}
