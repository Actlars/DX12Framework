// -------------------------------------------------------------------------------
//		Includes
// -------------------------------------------------------------------------------
#include "RenderQueue.h"
#include <Engine/Mesh/Mesh/Mesh.h>
#include <Engine/Renderer/SceneRenderer/SceneRenderer.h>

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
void RenderQueue::Execute(ID3D12GraphicsCommandList* _pCmd, RenderMode _mode)
{
	if (_pCmd == nullptr) 
	{ return; }

	for (auto& item : m_DrawItems)
	{
		if (item.pMesh == nullptr) 
		{ continue; }

		_pCmd->SetGraphicsRootConstantBufferView(item.TransformSlot, item.TransformCBAddress);

		if (_mode == RenderMode::Bindless)
		{
			_pCmd->SetGraphicsRootConstantBufferView(item.MaterialIndicesSlot, item.MaterialIndicesCBAddress);
		}
		else
		{
			_pCmd->SetGraphicsRootConstantBufferView(item.MaterialSlot, item.MaterialCBAddress);

			// TextureTableはTraditionalのみ
			// BindlessにはRootSignatureにTextureテーブルが存在しないので呼んではいけない
			if (item.HasTexture)
			{
				_pCmd->SetGraphicsRootDescriptorTable(item.TextureSlot, item.TextureHandle);
			}
		}

		item.pMesh->Draw(_pCmd);
	}
	m_DrawItems.clear();
}

size_t RenderQueue::GetItemCount() const
{
	return m_DrawItems.size();
}
