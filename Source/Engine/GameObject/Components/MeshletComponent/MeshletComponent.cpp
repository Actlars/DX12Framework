// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "MeshletComponent.h"
#include <Engine/GameObject/GameObject.h>
#include <Engine/GameObject/Components/TransformComponent/TransformComponent.h>
#include <Engine/Utility/Debug/Logger/Logger.h>
#include <Engine/RHI/Pipeline/RootSignature/RootSignatureLayout/RootSignatureLayout.h>
#include <Engine/Renderer/RenderQueue/MeshletRenderQueue/MeshletRenderQueue.h>
#include <Engine/RHI/Core/Device/Device.h>
#include <Engine/Mesh/MeshLoader/MeshLoader.h>

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
MeshletComponent::MeshletComponent()
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
MeshletComponent::~MeshletComponent() 
{ OnDetach(); }

// -------------------------------------------------------------------------------
//		初期化（モデルのロード、メッシュレット生成、定数バッファ確保）
// -------------------------------------------------------------------------------
bool MeshletComponent::Init(RHI::Device* _pDevice,const std::wstring& _modelPath)
{
	if (_pDevice == nullptr)
	{ return false; }

	auto* pDevice	= _pDevice->GetDevice();
	auto* pQueue	= _pDevice->GetQueue();
	auto* pResPool	= _pDevice->GetPool(RHI::Device::POOL_TYPE_RES);

	// モデルのロード
	std::vector<ResMesh>		resMeshes;
	std::vector<ResMaterial>	resMaterials;
	if (!MeshLoader::Load(_modelPath, resMeshes, resMaterials))
	{
		ELOG("MeshletComponent::Init() : MeshLoader::Load failed path = %ls", _modelPath.c_str());
		return false;
	}

	if (resMeshes.empty())
	{
		ELOG("MeshletComponent::Init() : resMeshes is empty");
		return false;
	}

	// マテリアル（テクスチャ）を生成
	m_Materials.reserve(resMaterials.size());
	for (auto& resMat : resMaterials)
	{
		auto mat = std::make_unique<Material>();
		if (!mat->Init(_pDevice, resMat))
		{
			ELOG("MeshletComponent::Init() : Material Init failed");
			return false;
		}
		m_Materials.emplace_back(std::move(mat));
	}

	// 全メッシュぶんMeshletResourceを作る
	m_Meshes.reserve(resMeshes.size());
	for (auto& resMesh : resMeshes)
	{
		auto meshlet = std::make_unique<MeshletResource>();
		if (!meshlet->InitMeshlets(pDevice, resMesh))
		{
			ELOG("MeshletComponent::Init() : InitMeshlets failed");
			continue;
		}

		ModelMeshletEntry entry;
		entry.Mesh = std::move(meshlet);

		// 対応するマテリアルからテクスチャIndexを取得
		const auto matId = resMesh.MaterialId;
		if (matId < m_Materials.size())
		{
			entry.DiffuseTextureIndex = m_Materials[matId]->GetTextureIndex(Material::TEXTURE_DIFFUSE);
		}

		m_Meshes.emplace_back(std::move(entry));
	}

	// フレームカウント分のTransformCBを作る
	const auto frameCount = _pDevice->GetFrameCount();
	m_TransformCBs.reserve(frameCount);
	for (auto i = 0u; i < frameCount; ++i)
	{
		auto cb = std::make_unique<RHI::ConstantBuffer>();
		if (!cb->Init(pDevice, pResPool, sizeof(TransformCB)))
		{
			ELOG("MeshletComponent::Init() : TransformCB[%u] Init failed", i);
			return false;
		}
		m_TransformCBs.emplace_back(std::move(cb));
	}

	return true;
}

// -------------------------------------------------------------------------------
//		デタッチ時の処理（定数バッファの解放）
// -------------------------------------------------------------------------------
void MeshletComponent::OnDetach()
{
	m_TransformCBs.clear();
	m_Materials.clear();
	m_Meshes.clear();
}

// -------------------------------------------------------------------------------
//		カメラの行列（GameSceneから毎フレーム呼ぶ）
// -------------------------------------------------------------------------------
void MeshletComponent::SetViewProj(
	const DirectX::XMMATRIX& _view,
	const DirectX::XMMATRIX& _proj)
{
	m_View = _view;
	m_Proj = _proj;
}

bool MeshletComponent::IsVisible() const
{
	return m_IsVisible && !m_Meshes.empty();
}

void MeshletComponent::SetFrameIndex(uint32_t _frameIndex)
{
	m_FrameIndex = _frameIndex;
}

// -------------------------------------------------------------------------------
//		RootSignatureLayoutを設定
// -------------------------------------------------------------------------------
void MeshletComponent::SetRootLayout(const RHI::RootSignatureLayout* _pRootLayout)
{
	if (_pRootLayout == nullptr)
	{
		return;
	}

	m_TransformSlot = _pRootLayout->GetSlot("Transform");
	m_TextureIndexSlot = _pRootLayout->GetSlot("TextureIndex");

	const uint32_t verticesSlot = _pRootLayout->GetSlot("Vertices");
	const uint32_t meshletVerticesSlot = _pRootLayout->GetSlot("MeshletVertexIndices");
	const uint32_t primitiveIndicesSlot = _pRootLayout->GetSlot("PackedPrimitiveIndices");
	const uint32_t meshletsSlot = _pRootLayout->GetSlot("Meshlets");

	for (auto& entry : m_Meshes)
	{
		if (entry.Mesh)
		{
			entry.Mesh->SetMeshletRootSlots(
				verticesSlot, meshletVerticesSlot, primitiveIndicesSlot, meshletsSlot);
		}
	}
}

void MeshletComponent::SetVisible(bool _visible)
{
	m_IsVisible = _visible;
}

// -------------------------------------------------------------------------------
//		描画コマンドを積む
// -------------------------------------------------------------------------------
void MeshletComponent::Submit(MeshletRenderQueue* _pQueue)
{
	if (_pQueue == nullptr || m_Meshes.empty()) 
	{ return; }

	// TransformComponentからワールド行列を取得
	DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
	if (m_pOwner != nullptr)
	{
		auto* pTransform = m_pOwner->GetComponent<TransformComponent>();
		if (pTransform != nullptr) { world = pTransform->GetWorldMatrix(); }
	}

	// TransformCBへの書き込みは全メッシュ共通なので一回だけでよい
	D3D12_GPU_VIRTUAL_ADDRESS transformAddr = 0;
	if (m_FrameIndex < m_TransformCBs.size() && m_TransformCBs[m_FrameIndex])
	{
		auto* pCB	= m_TransformCBs[m_FrameIndex]->GetPtr<TransformCB>();
		pCB->World	= world;
		pCB->View	= m_View;
		pCB->Proj	= m_Proj;

		transformAddr = m_TransformCBs[m_FrameIndex]->GetAddress();
	}

	for (auto& entry : m_Meshes)
	{
		MeshletDrawItem item;
		item.pMesh					= entry.Mesh.get();
		item.DiffuseTextureIndex	= entry.DiffuseTextureIndex;
		item.TransformCBAddress		= transformAddr;
		item.TransformSlot			= m_TransformSlot;
		item.TextureIndexSlot		= m_TextureIndexSlot;

		_pQueue->Submit(item);
	}
}
