// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Mesh.h"
#include <Framework/Mesh/PolygonMeshResource/PolygonMeshResource.h>
#include <Framework/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
Mesh::Mesh()
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
Mesh::~Mesh() 
{ Term(); }

// -------------------------------------------------------------------------------
//		従来パイプラインで初期化
// -------------------------------------------------------------------------------
bool Mesh::Init(ID3D12Device* _pDevice, const ResMesh& _resMesh)
{
	if (_pDevice == nullptr) 
	{ return false; }

	// PolygonMeshResourceを生成してMeshResourceとして保持
	auto resource = std::make_unique<PolygonMeshResource>();
	if (!resource->Init(_pDevice, _resMesh))
	{
		ELOG("Mesh::Init() PolygonMeshResource::Init() failed");
		return false;
	}

	m_pResource		= std::move(resource);
	m_MaterialId	= _resMesh.MaterialId;

	return true;
}

// -------------------------------------------------------------------------------
//		任意のMeshResourceで初期化（メッシュシェーダー等）
// -------------------------------------------------------------------------------
void Mesh::InitWithResource(
	std::unique_ptr<MeshResource>	_resource,
	uint32_t						_materialId)
{
	m_pResource		= std::move(_resource);
	m_MaterialId	= _materialId;
}

// -------------------------------------------------------------------------------
//		終了処理
// -------------------------------------------------------------------------------
void Mesh::Term()
{
	m_pResource.reset();
	m_MaterialId = 0;
}

// -------------------------------------------------------------------------------
//		描画コマンドを積む
// -------------------------------------------------------------------------------
void Mesh::Draw(
	ID3D12GraphicsCommandList*	_pCmd,
	uint32_t					_instanceCount) const
{
	if (m_pResource == nullptr) 
	{ return; }

	m_pResource->Draw(_pCmd, _instanceCount);
}

// -------------------------------------------------------------------------------
//		ゲッター
// -------------------------------------------------------------------------------
uint32_t Mesh::GetMaterialId() const
{ return m_MaterialId; }

bool Mesh::IsValid() const 
{ return m_pResource != nullptr; }

bool Mesh::NeedsInputLayout() const 
{ return m_pResource ? m_pResource->NeedsInputLayout() : false; }

uint32_t Mesh::GetIndexCount() const 
{ return m_pResource ? m_pResource->GetIndexCount() : 0; }

uint32_t Mesh::GetVertexCount() const 
{ return m_pResource ? m_pResource->GetVertexCount() : 0; }
