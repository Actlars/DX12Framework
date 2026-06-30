// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "FPSCamera.h"

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
FPSCamera::FPSCamera()
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
FPSCamera::~FPSCamera()
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		初期化
// -------------------------------------------------------------------------------
void FPSCamera::Init(const Desc& _desc)
{
	m_Position	= _desc.Position;
	m_Yaw		= _desc.Yaw;
	m_Pitch		= _desc.Pitch;
	m_MoveSpeed = _desc.MoveSpeed;
	m_RotSpeed	= _desc.RotSpeed;

	// 初期状態の方向ベクトルとビュー行列を計算しておく
	// プロジェクション行列は基底クラスのUpdateProj()が管理する
	UpdateVectors();
	Update();
}

// -------------------------------------------------------------------------------
//		毎フレーム更新
// -------------------------------------------------------------------------------
void FPSCamera::Update()
{
	// Pitchのクランプ
	// 真上・真下を向くと行列が不安定になるので +-89 度に制限する
	m_Pitch = std::clamp(m_Pitch, PITCH_MIN, PITCH_MAX);

	// 方向ベクトルの再計算
	UpdateVectors();

	// ビュー行列の再計算
	// Position + Forward から LookAt 行列を生成する
	// LookAt(eye, target, up) の target は eye + forward
	const auto eye		= DirectX::XMLoadFloat3(&m_Position);
	const auto fwd		= DirectX::XMLoadFloat3(&m_Forward);
	const auto worldUp	= DirectX::XMLoadFloat3(&WORLD_UP);
	const auto target	= DirectX::XMVectorAdd(eye, fwd);

	m_View = DirectX::XMMatrixLookAtLH(eye, target, worldUp);
}

// ビュー行列を返す
DirectX::XMMATRIX FPSCamera::GetView() const
{ return m_View; }

// プロジェクション行列を返す（基底クラスのキャッシュを返す）
DirectX::XMMATRIX FPSCamera::GetProj() const
{ return m_Proj; }

// 現在位置を返す
DirectX::XMFLOAT3 FPSCamera::GetPosition() const 
{ return m_Position; }

// -------------------------------------------------------------------------------
//		前進
// -------------------------------------------------------------------------------
void FPSCamera::MoveForward(float _deltaTime)
{
	// Forward ベクトル方向に MoveSpeed * DeltaTimeだけ移動する
	const auto pos = DirectX::XMLoadFloat3(&m_Position);
	const auto fwd = DirectX::XMLoadFloat3(&m_Forward);
	const auto vel = DirectX::XMVectorScale(fwd, m_MoveSpeed * _deltaTime);
	DirectX::XMStoreFloat3(&m_Position, DirectX::XMVectorAdd(pos, vel));
}

// -------------------------------------------------------------------------------
//		後退
// -------------------------------------------------------------------------------
void FPSCamera::MoveBack(float _deltaTime)
{
	const auto pos = DirectX::XMLoadFloat3(&m_Position);
	const auto fwd = DirectX::XMLoadFloat3(&m_Forward);
	const auto vel = DirectX::XMVectorScale(fwd, m_MoveSpeed * _deltaTime);
	// 進行方向と逆
	DirectX::XMStoreFloat3(&m_Position, DirectX::XMVectorSubtract(pos, vel));
}

// -------------------------------------------------------------------------------
//		左
// -------------------------------------------------------------------------------
void FPSCamera::MoveLeft(float _deltaTime)
{
	const auto pos		= DirectX::XMLoadFloat3(&m_Position);
	const auto right	= DirectX::XMLoadFloat3(&m_Right);
	const auto vel		= DirectX::XMVectorScale(right, m_MoveSpeed * _deltaTime);
	DirectX::XMStoreFloat3(&m_Position, DirectX::XMVectorSubtract(pos, vel));
}

// -------------------------------------------------------------------------------
//		右
// -------------------------------------------------------------------------------
void FPSCamera::MoveRight(float _deltaTime)
{
	const auto pos = DirectX::XMLoadFloat3(&m_Position);
	const auto right = DirectX::XMLoadFloat3(&m_Right);
	const auto vel = DirectX::XMVectorScale(right, m_MoveSpeed * _deltaTime);
	DirectX::XMStoreFloat3(&m_Position, DirectX::XMVectorAdd(pos, vel));
}

// -------------------------------------------------------------------------------
//		 上昇（ワールドY軸方向）
// -------------------------------------------------------------------------------
void FPSCamera::MoveUp(float _deltaTime)
{
	m_Position.y += m_MoveSpeed * _deltaTime;
}

// -------------------------------------------------------------------------------
//		下降（ワールドY軸方向）
// -------------------------------------------------------------------------------
void FPSCamera::MoveDown(float _deltaTime)
{
	m_Position.y -= m_MoveSpeed * _deltaTime;
}

// -------------------------------------------------------------------------------
//		左右回転
// -------------------------------------------------------------------------------
void FPSCamera::AddYaw(float _deltaTime)
{
	m_Yaw += m_RotSpeed * _deltaTime;

	// 0 ～ 360 度の範囲に正規化（オーバーフロー防止）
	if (m_Yaw > 360.0f)	 { m_Yaw -= 360.0f; }
	if (m_Yaw < -360.0f) { m_Yaw += 360.0f; }
}

// -------------------------------------------------------------------------------
//		上下回転
// -------------------------------------------------------------------------------
void FPSCamera::AddPitch(float _deltaTime)
{
	// クランプはUpdate()で行うのでここでは加算のみ行う
	m_Pitch += m_RotSpeed * _deltaTime;
}

// -------------------------------------------------------------------------------
//		位置の直接設定
// -------------------------------------------------------------------------------
void FPSCamera::SetPosition(const DirectX::XMFLOAT3& _pos)
{ m_Position = _pos; }

// -------------------------------------------------------------------------------
//		回転の直接設定
// -------------------------------------------------------------------------------
void FPSCamera::SetRotation(float _yaw, float _pitch) 
{
	m_Yaw = _yaw;
	m_Pitch = std::clamp(_pitch, PITCH_MIN, PITCH_MAX);
}

// -------------------------------------------------------------------------------
//		方向ベクトルの再計算
// -------------------------------------------------------------------------------
// Yaw / Pitch（度）から Forward / Right ベクトルを求める
// 
// 計算方法（右手座標系） : 
//	Yaw		= Y軸回りの回転（正 = 右回転）
//	Pitch	= X軸回りの回転（正 = 下向き）
// 
//	Forward.x = cos(Pitch) * sin(Yaw)
//	Forward.y = -sin(Pitch)		負にすることで正のPitch = 下向き
// 
//	Forward.z = -cos(Pitch) * cos(Yaw)	右手系なのでZ方向は負
// 
//  Right = normalize(Forward * WorldUp) 外積で右方向を求める
// -------------------------------------------------------------------------------
void FPSCamera::UpdateVectors()
{
	const float yawRad		= DirectX::XMConvertToRadians(m_Yaw);
	const float pitchRad	= DirectX::XMConvertToRadians(m_Pitch);

	// Forwardベクトル
	DirectX::XMFLOAT3 fwd;
	fwd.x = cosf(pitchRad) * sinf(yawRad);
	fwd.y = -sinf(pitchRad);
	fwd.z = cosf(pitchRad) * cosf(yawRad);

	DirectX::XMStoreFloat3(
		&m_Forward,
		DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&fwd)));
	
	// Rightベクトル
	// Forward * WorldUp の外積を正規化
	// WorldUp と Forward が平行（真上/真下を向く）になると外積が０になるため
	// Pitchを±89度にクランプすることで発生しない
	const auto worldUp = DirectX::XMLoadFloat3(&WORLD_UP);
	const auto forward = DirectX::XMLoadFloat3(&m_Forward);

	DirectX::XMStoreFloat3(
		&m_Right,
		DirectX::XMVector3Normalize(DirectX::XMVector3Cross(worldUp, forward)));
}

