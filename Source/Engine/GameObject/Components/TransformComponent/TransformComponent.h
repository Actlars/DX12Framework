#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Engine/GameObject/Component/Component.h"

// -------------------------------------------------------------------------------
// TransformComponent class
// 
// 概要 : 
//	GameObjectの位置・回転スケールを管理するコンポーネント
//	IRenderableは継承しない（描画コマンドが必要ないため）
// 
//	MeshComponent は TransformComponent からGetWorldMatrix()を取得して定数バッファに渡す
// -------------------------------------------------------------------------------
class TransformComponent : public Component
{
public:

	// -------------------------------------------------------------------------------
	// コンストラクタ
	// -------------------------------------------------------------------------------
	TransformComponent();

	// -------------------------------------------------------------------------------
	// デストラクタ
	// -------------------------------------------------------------------------------
	~TransformComponent();

	// -------------------------------------------------------------------------------
	// Componentインターフェースの実装
	// -------------------------------------------------------------------------------

	// @brief	毎フレームの更新（TransformComponentは状態管理のみで更新しない）
	void Update(float _deltaTime) override;

	// -------------------------------------------------------------------------------
	// Transform 操作
	// -------------------------------------------------------------------------------
	void SetPosition(const DirectX::XMFLOAT3& _pos);
	void SetRotation(const DirectX::XMFLOAT3& _rot);
	void SetScale	(const DirectX::XMFLOAT3& _scale);

	void AddPosition(const DirectX::XMFLOAT3& _delta);
	void AddRotation(const DirectX::XMFLOAT3& _delta);

	// -------------------------------------------------------------------------------
	// ゲッター
	// -------------------------------------------------------------------------------
	const DirectX::XMFLOAT3& GetPosition()	const;
	const DirectX::XMFLOAT3& GetRotation()	const;
	const DirectX::XMFLOAT3& GetScale()		const;

	// -------------------------------------------------------------------------------
	// @breif	ワールド変換行列を返す
	//			Dirtyフラグが立っているときのみ再計算する
	// -------------------------------------------------------------------------------
	DirectX::XMMATRIX GetWorldMatrix() const;

private:

	// -------------------------------------------------------------------------------
	// private variables
	// -------------------------------------------------------------------------------
	DirectX::XMFLOAT3 m_Position	= { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_Rotation	= { 0.0f, 0.0f, 0.0f };	// オイラー角（度）
	DirectX::XMFLOAT3 m_Scale		= { 1.0f,1.0f,1.0f };

	// キャッシュ済みワールド行列（Dirtyフラグで再計算を最小化）
	mutable DirectX::XMMATRIX	m_WorldMatrix = DirectX::XMMatrixIdentity();
	mutable bool				m_Dirty = false;
};
