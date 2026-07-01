// -------------------------------------------------------------------------------
//		Includes
// -------------------------------------------------------------------------------
#include "RenderQueue.h"
#include <Engine/Mesh/Mesh/Mesh.h>

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
RenderQueue::RenderQueue()
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
RenderQueue::~RenderQueue()
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		DrawItemを積む
// -------------------------------------------------------------------------------
void RenderQueue::Submit(const DrawItem& _item)
{
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_DrawItems.emplace_back(_item);
}

// -------------------------------------------------------------------------------
//		積まれたDrawItemを順にコマンドリストは発行
// -------------------------------------------------------------------------------
void RenderQueue::Execute(ID3D12GraphicsCommandList* _pCmd)
{
	if (_pCmd == nullptr) 
	{ return; }

	for (auto& item : m_DrawItems)
	{
		if (item.pMesh == nullptr) 
		{ continue; }

		_pCmd->SetGraphicsRootConstantBufferView(item.TransformSlot, item.TransformCBAddress);
		_pCmd->SetGraphicsRootConstantBufferView(item.MaterialSlot, item.MaterialCBAddress);

		if (item.HasTexture)
		{
			_pCmd->SetGraphicsRootDescriptorTable(item.TextureSlot, item.TextureHandle);
		}

		item.pMesh->Draw(_pCmd);
	}
	m_DrawItems.clear();
}

size_t RenderQueue::GetItemCount() const
{
	return m_DrawItems.size();
}
