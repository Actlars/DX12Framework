// -------------------------------------------------------------------------------
//		Includes
// -------------------------------------------------------------------------------
#include "MeshletRenderQueue.h"
#include <Engine/Mesh/Mesh/Mesh.h>
#include <Engine/Renderer/SceneRenderer/SceneRenderer.h>
#include <Engine/Mesh/MeshletResource/MeshletResource.h>

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
MeshletRenderQueue::MeshletRenderQueue()
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
MeshletRenderQueue::~MeshletRenderQueue()
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		DrawItemを積む
// -------------------------------------------------------------------------------
void MeshletRenderQueue::Submit(const MeshletDrawItem& _item)
{
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_DrawItems.emplace_back(_item);
}

// -------------------------------------------------------------------------------
//		積まれたDrawItemを順にコマンドリストは発行
// -------------------------------------------------------------------------------
void MeshletRenderQueue::Execute(ID3D12GraphicsCommandList* _pCmd)
{
	if (_pCmd == nullptr) 
	{ return; }

	for (auto& item : m_DrawItems)
	{
		if (item.pMesh == nullptr) 
		{ continue; }

		if (item.TransformSlot != UINT32_MAX)
		{
			_pCmd->SetGraphicsRootConstantBufferView(item.TransformSlot, item.TransformCBAddress);
		}

		if (item.TextureIndexSlot != UINT32_MAX)
		{
			_pCmd->SetGraphicsRoot32BitConstant(item.TextureIndexSlot, item.DiffuseTextureIndex, 0);
		}

		item.pMesh->Draw(_pCmd);
	}
	m_DrawItems.clear();
}

size_t MeshletRenderQueue::GetItemCount() const
{
	return m_DrawItems.size();
}
