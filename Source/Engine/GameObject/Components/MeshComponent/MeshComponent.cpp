// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "MeshComponent.h"
#include <Engine/GameObject/GameObject.h>
#include <Engine/GameObject/Components/TransformComponent/TransformComponent.h>
#include <Engine/Utility/Debug/Logger/Logger.h>
#include <Engine/RootSignature/RootSignatureLayout/RootSignatureLayout.h>
#include <Engine/Renderer/RenderQueue/RenderQueue.h>

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
MeshComponent::MeshComponent()
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
MeshComponent::~MeshComponent() 
{ OnDetach(); }

// -------------------------------------------------------------------------------
//		定数バッファの初期化
// -------------------------------------------------------------------------------
bool MeshComponent::Init(
	ID3D12Device* _pDevice,
	DescriptorPool* _pPool,
	uint32_t _frameCount)
{
	if (_pDevice == nullptr || _pPool == nullptr || _frameCount == 0)
	{ return false; }

	// FrameCount分の定数バッファを作成する
	m_TransformCBs.reserve(_frameCount);
	for (auto i = 0u; i < _frameCount; ++i)
	{
		auto cb = std::make_unique<ConstantBuffer>();
		if (!cb->Init(_pDevice, _pPool, sizeof(TransformCB)))
		{
			ELOG("MeshComponent::Init() ConstantBuffer[%u] failed", i);
			return false;
		}
		m_TransformCBs.emplace_back(std::move(cb));
	}

	return true;
}

// -------------------------------------------------------------------------------
//		デタッチ時の処理（定数バッファの解放）
// -------------------------------------------------------------------------------
void MeshComponent::OnDetach()
{
	m_TransformCBs.clear();
	m_pMesh		= nullptr;
	m_pMaterial = nullptr;
}

// -------------------------------------------------------------------------------
//		メッシュとマテリアルの設定
// -------------------------------------------------------------------------------
void MeshComponent::SetMesh(Mesh* _pMesh, Material* _pMaterial)
{
	m_pMesh		= _pMesh;
	m_pMaterial = _pMaterial;
}

// -------------------------------------------------------------------------------
//		RootParameterのスロット番号
// -------------------------------------------------------------------------------
void MeshComponent::SetRootParamSlots(
	uint32_t _transformSlot,
	uint32_t _materialSlot,
	uint32_t _textureSlot)
{
	m_TransformSlot = _transformSlot;
	m_MaterialSlot	= _materialSlot;
	m_TextureSlot	= _textureSlot;
}

// -------------------------------------------------------------------------------
//		カメラの行列（GameSceneから毎フレーム呼ぶ）
// -------------------------------------------------------------------------------
void MeshComponent::SetViewProj(
	const DirectX::XMMATRIX& _view,
	const DirectX::XMMATRIX& _proj)
{
	m_View = _view;
	m_Proj = _proj;
}

bool MeshComponent::IsVisible() const
{
	return m_IsValiable && m_pMesh != nullptr;
}
void MeshComponent::SetFrameIndex(uint32_t _frameIndex)
{
	m_FrameIndex = _frameIndex;
}

// -------------------------------------------------------------------------------
//		RootSignatureLayoutを設定
// -------------------------------------------------------------------------------
void MeshComponent::SetRootLayout(const RootSignatureLayout* _pRootLayout)
{
	if (_pRootLayout == nullptr) 
	{ return; }

	// スロット番号を名前から取得する
	m_TransformSlot = _pRootLayout->GetSlot("Transform");
	m_MaterialSlot	= _pRootLayout->GetSlot("Material");
	m_TextureSlot	= _pRootLayout->GetSlot("Texture");
}

void MeshComponent::SetVisible(bool _visible)
{
	m_IsValiable = _visible;
}

// -------------------------------------------------------------------------------
// 
// -------------------------------------------------------------------------------
void MeshComponent::Submit(RenderQueue* _pQueue)
{
	if (m_pMesh == nullptr) 
	{ return; }

	// TransformComponentからワールド行列を取得
	DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
	if (m_pOwner != nullptr)
	{
		auto* pTransform = m_pOwner->GetComponent<TransformComponent>();
		if (pTransform != nullptr) { world = pTransform->GetWorldMatrix(); }
	}

	DrawItem item;
	item.pMesh			= m_pMesh;
	item.TransformSlot	= m_TransformSlot;
	item.MaterialSlot	= m_MaterialSlot;
	item.TextureSlot	= m_TextureSlot;

	// TransformCBへの書き込み memcpyなのでDX12APIではないので安全に並列化できる
	if (m_FrameIndex < m_TransformCBs.size() && m_TransformCBs[m_FrameIndex])
	{
		auto* pCB	= m_TransformCBs[m_FrameIndex]->GetPtr<TransformCB>();
		pCB->World	= world;
		pCB->View	= m_View;
		pCB->Proj	= m_Proj;

		item.TransformCBAddress = m_TransformCBs[m_FrameIndex]->GetAddress();
	}

	if (m_pMaterial != nullptr)
	{
		item.MaterialCBAddress = m_pMaterial->GetCBAddress();

		const auto texHandle = m_pMaterial->GetTextureHandle(Material::TEXTURE_DIFFUSE);
		if (texHandle.ptr != 0)
		{
			item.TextureHandle	= texHandle;
			item.HasTexture		= true;
		}
	}

	_pQueue->Submit(item);
}

//// -------------------------------------------------------------------------------
////		描画コマンドを積む
//// -------------------------------------------------------------------------------
//void MeshComponent::Draw(ID3D12GraphicsCommandList* _pCmd)
//{
//	if (_pCmd == nullptr || m_pMesh == nullptr) 
//	{ return; }
//
//	// TransformComponent からワールド行列を取得
//	// GetComponentを通じてアクセスする（直接参照しない）
//	DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
//	if (m_pOwner != nullptr)
//	{
//		auto* pTransform = m_pOwner->GetComponent<TransformComponent>();
//		if (pTransform != nullptr) 
//		{ world = pTransform->GetWorldMatrix(); }
//	}
//
//	if (m_FrameIndex < m_TransformCBs.size() && m_TransformCBs[m_FrameIndex])
//	{
//		auto* pCB = m_TransformCBs[m_FrameIndex]->GetPtr<TransformCB>();
//		pCB->World = world;
//		pCB->View = m_View;
//		pCB->Proj = m_Proj;
//
//		_pCmd->SetGraphicsRootConstantBufferView(
//			m_TransformSlot,
//			m_TransformCBs[m_FrameIndex]->GetAddress());
//	}
//
//	// マテリアルの定数バッファをバインド
//	if (m_pMaterial != nullptr)
//	{
//		_pCmd->SetGraphicsRootConstantBufferView(
//			m_MaterialSlot,
//			m_pMaterial->GetCBAddress());
//
//		const auto texHandle = m_pMaterial->GetTextureHandle(Material::TEXTURE_DIFFUSE);
//		if (texHandle.ptr != 0)
//		{
//			_pCmd->SetGraphicsRootDescriptorTable(
//				m_TextureSlot,
//				texHandle);
//		}
//	}
//
//	// 描画コマンドを積む
//	m_pMesh->Draw(_pCmd);
//}
