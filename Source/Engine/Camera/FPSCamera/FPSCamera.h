#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "../CameraBase.h"

// -------------------------------------------------------------------------------
// FPSCamra class 
// 
// 概要 : 
//	WASD + マウスで自由移動するFPSカメラ
//	CameraBaseを継承し、Update, GetView, GetProj, GetPositionを実装する
// 
//	入力処理は持たない
//	Sceneなどから移動・回転の命令を受け取って状態を更新する
// 
// 座標系 : 
//	左手座標系
//	Y軸が上、カメラ初期方向は Z方向
// 
// 使い方 : 
//	auto pCamera = std::make_unique<FPSCamera>();
// 
//	FPSCamera::Desc desc;
//	desc.Position = { 0.0f, 100.0f, 300.0f};
//	desc.MoveSpped = 100.0f;
//	desc.RotSpeed = 0.2f;
//	pCamera->Init(desc);
//	pCamera->SetAspect(1280.0f / 720.0f);
//	pCamera->SetFov(45.0f);
// 
//	毎フレーム
//	if(Wキー) pCamera->MoveForward(deltaTime);
//	pCamera->AddYaw(mouseDeltaX);
//	pCamera->AddPitch(mouseDeltaY);
//	pCamera->Update();
//	
//	pCB->View = pCamera->GetView();
//	pCB->Proj = pCamera->GetProj();
// -------------------------------------------------------------------------------
class FPSCamera final : public CameraBase
{
public:

	// -------------------------------------------------------------------------------
	// 初期パラメータ（FPSCamera固有のもの）
	// プロジェクション設定は基底クラスのものを使う
	// -------------------------------------------------------------------------------
	struct Desc
	{
		DirectX::XMFLOAT3	Position	= { 0.0f,1.0f,5.0f };	// 初期位置
		float				Yaw			= 0.0f;					// 初期左右角度（度）
		float				Pitch		= 0.0f;					// 初期上下角度（度）
		float				MoveSpeed	= 10.0f;				// 移動速度（単位/秒）
		float				RotSpeed	= 0.1f;					// 回転角度（度/入力単位）
	};

	// -------------------------------------------------------------------------------
	// コンストラクタ
	// -------------------------------------------------------------------------------
	FPSCamera();

	// -------------------------------------------------------------------------------
	// デストラクタ
	// -------------------------------------------------------------------------------
	~FPSCamera();

	// -------------------------------------------------------------------------------
	// @brief	初期化
	// @param[in]	_desc	FPSCamera固有の初期化パラメータ
	// -------------------------------------------------------------------------------
	void Init(const Desc& _desc);

	// -------------------------------------------------------------------------------
	// CameraBase インターフェースの実装
	// -------------------------------------------------------------------------------

	// @brief	Yaw / Pitch から Forward/ Right を再計算してビュー行列を更新する
	void Update()override;

	// @brief	ビュー行列を返す
	DirectX::XMMATRIX GetView() const override;

	// @brief	プロジェクション行列を返す
	DirectX::XMMATRIX GetProj() const override;

	// @brief	現在位置を返す
	DirectX::XMFLOAT3 GetPosition() const override;

	// -------------------------------------------------------------------------------
	// 移動命令
	// -------------------------------------------------------------------------------
	
	// @brief	前進（カメラが向いている方向）
	void MoveForward(float _deltaTime);

	// @brief	後退（カメラが向いている方向の逆）
	void MoveBack(float _deltaTime);

	// @brief	左
	void MoveLeft(float _deltaTime);

	// @brief	右
	void MoveRight(float _deltaTime);

	// @brief	上昇（ワールドY軸方向）
	void MoveUp(float _deltaTime);

	// @brief	下降（ワールドY軸方向）
	void MoveDown(float _deltaTime);

	// -------------------------------------------------------------------------------
	// 回転命令
	// -------------------------------------------------------------------------------

	// @brief	左右回転（Y軸回り）。正の値で右回転
	void AddYaw(float _deltaTime);

	// @brief	上下回転（X軸回り）。正の値で下向き
	void AddPitch(float _deltaTime);

	// -------------------------------------------------------------------------------
	// 直接設定
	// -------------------------------------------------------------------------------

	// @brief	位置を直接設定する（カットシーンやリスポーン時に使う）
	void SetPosition(const DirectX::XMFLOAT3& _pos);

	// @brief	Yaw / Pitchを直接設定する（度単位）
	void SetRotation(float _yaw, float _pitch);

	// @brief	移動速度を変更する
	void SetMoveSpeed(float _speed) { m_MoveSpeed = _speed; }

	// @brief	回転感度を変更する
	void SetRotSpeed(float _speed) { m_RotSpeed = _speed; }

	// ゲッター
	float				GetYaw()		const;
	float				GetPitch()		const;
	float				GetMoveSpeed()	const;
	float				GetRotSpeed()	const;
	DirectX::XMFLOAT3	GetForward()	const;
	DirectX::XMFLOAT3	GetRight()		const;

private:

	// -------------------------------------------------------------------------------
	// @brief	Yaw / Pitch から Forward / Right ベクトルを再計算する
	// -------------------------------------------------------------------------------
	void UpdateVectors();

	// -------------------------------------------------------------------------------
	// private variables
	// -------------------------------------------------------------------------------

	// 位置・姿勢
	DirectX::XMFLOAT3	m_Position	= { 0.0f,0.0f,-5.0f };
	float				m_Yaw		= 0.0f;	// 左右角度（度）
	float				m_Pitch		= 0.0f;	// 上下角度（度）

	// 方向ベクトル（UpdateVectors()で毎フレーム再計算）
	DirectX::XMFLOAT3 m_Forward		= { 0.0f,0.0f,-1.0f };
	DirectX::XMFLOAT3 m_Right		= { 1.0f,0.0f,0.0f };

	// 上方向はワールド Y 軸固定
	static constexpr DirectX::XMFLOAT3 WORLD_UP = { 0.0f,1.0f,0.0f };

	// 速度設定
	float m_MoveSpeed	= 2.0f;
	float m_RotSpeed	= 0.1f;

	// キャッシュ済みビュー行列
	DirectX::XMMATRIX m_View = DirectX::XMMatrixIdentity();

	// Pitchのクランプ範囲（真上・真下を向くとビュー行列が不安定になる）
	static constexpr float PITCH_MAX = 89.0f;
	static constexpr float PITCH_MIN = -89.0f;

};
