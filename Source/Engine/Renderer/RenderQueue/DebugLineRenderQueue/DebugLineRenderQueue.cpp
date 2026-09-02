// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "DebugLineRenderQueue.h"

// -------------------------------------------------------------------------------
// コンストラクタ
// -------------------------------------------------------------------------------
DebugLineRenderQueue::DebugLineRenderQueue()
{ /* DO_NOTHING */
}

// -------------------------------------------------------------------------------
// デストラクタ
// -------------------------------------------------------------------------------
DebugLineRenderQueue::~DebugLineRenderQueue()
{
	Term();
}

// -------------------------------------------------------------------------------
// 初期化
// -------------------------------------------------------------------------------
bool DebugLineRenderQueue::Init(
	ID3D12Device* _pDevice, uint32_t _frameCount
)
{
	if (_pDevice == nullptr)
	{
		return false;
	}

	Term();

	m_DrawItems.reserve(kMaxLineCount);
	m_Vertices.reserve(kMaxVertexCount);

	return m_LineResource.Init(_pDevice,_frameCount, kMaxVertexCount);
}

// -------------------------------------------------------------------------------
// 終了
// -------------------------------------------------------------------------------
void DebugLineRenderQueue::Term()
{
	m_DrawItems.clear();
	m_Vertices.clear();

	m_LineResource.Term();
}

// -------------------------------------------------------------------------------
// DrawItemを積む
// -------------------------------------------------------------------------------
void DebugLineRenderQueue::Submit(
	const DebugLineDrawItem& _item
)
{
	//std::lock_guard<std::mutex> lock(m_Mutex);

	if (m_DrawItems.size() >= kMaxLineCount)
	{
		return;
	}

	m_DrawItems.emplace_back(_item);
}

void DebugLineRenderQueue::SubmitLine(
	const Math::Vector3& _start, 
	const Math::Vector3& _end, 
	const Math::Vector4& _color)
{
	Submit(
		DebugLineDrawItem
		{
			_start,
			_end,
			_color
		}
	);
}

// -------------------------------------------------------------------------------
// AABBを積む
// -------------------------------------------------------------------------------
void DebugLineRenderQueue::SubmitAABB(
	const Math::Vector3& _center,
	const Math::Vector3& _halfExtents,
	const Math::Vector4& _color
)
{
	const Math::Vector3 min =
		_center - _halfExtents;

	const Math::Vector3 max =
		_center + _halfExtents;

	const Math::Vector3 vertices[8]
	{
		{ min.x, min.y, min.z },
		{ max.x, min.y, min.z },
		{ max.x, min.y, max.z },
		{ min.x, min.y, max.z },

		{ min.x, max.y, min.z },
		{ max.x, max.y, min.z },
		{ max.x, max.y, max.z },
		{ min.x, max.y, max.z },
	};

	//std::lock_guard<std::mutex> lock(m_Mutex);

	// AABB1個につき１２ライン必要
	if (m_DrawItems.size() + 12 > kMaxLineCount)
	{ return; }

	const auto addLine = [this, &_color](
		const Math::Vector3& _start, const Math::Vector3& _end)
		{
			m_DrawItems.emplace_back(
				DebugLineDrawItem
				{
					_start,
					_end,
					_color
				});
		};

	// 底面
	Submit({ vertices[0], vertices[1], _color });
	Submit({ vertices[1], vertices[2], _color });
	Submit({ vertices[2], vertices[3], _color });
	Submit({ vertices[3], vertices[0], _color });

	// 上面
	Submit({ vertices[4], vertices[5], _color });
	Submit({ vertices[5], vertices[6], _color });
	Submit({ vertices[6], vertices[7], _color });
	Submit({ vertices[7], vertices[4], _color });

	// 縦方向
	Submit({ vertices[0], vertices[4], _color });
	Submit({ vertices[1], vertices[5], _color });
	Submit({ vertices[2], vertices[6], _color });
	Submit({ vertices[3], vertices[7], _color });
}

// -------------------------------------------------------------------------------
// 積まれたDebugLineを描画
// -------------------------------------------------------------------------------
void DebugLineRenderQueue::Execute(ID3D12GraphicsCommandList* _pCmd, uint32_t _frameIndex)
{
	if (_pCmd == nullptr)
	{
		return;
	}

	// Submit側と描画側を分離
	// lockを保持したままGPUバッファ更新を行わない
	std::vector<DebugLineDrawItem> drawItems;

	{
		//std::lock_guard<std::mutex> lock(m_Mutex);
		drawItems.swap(m_DrawItems);
	}

	m_Vertices.clear();

	// -------------------------------------------------------------------------------
	// DrawItem
	//
	// 1 DrawItem
	//	↓
	// Start / End の2頂点へ変換
	// -------------------------------------------------------------------------------
	for (const auto& item : drawItems)
	{
		if (m_Vertices.size() + 2 > kMaxVertexCount)
		{
			break;
		}

		m_Vertices.emplace_back(
			DebugLineVertex
			{
				item.Start,
				item.Color
			});

		m_Vertices.emplace_back(
			DebugLineVertex
			{
				item.End,
				item.Color
			});
	}

	// GPUバッファへ転送
	m_LineResource.UpdateVertexBuffer(
		_frameIndex,
		m_Vertices.data(),
		static_cast<uint32_t>(m_Vertices.size()));

	// -------------------------------------------------------------------------------
	// すべてのDebugLineを一度のDrawCallで描画
	// -------------------------------------------------------------------------------
	m_LineResource.Draw(_pCmd, _frameIndex);
}

// -------------------------------------------------------------------------------
// Item数取得
// -------------------------------------------------------------------------------
size_t DebugLineRenderQueue::GetItemCount() const
{
	//std::lock_guard<std::mutex> lock(m_Mutex);
	return m_DrawItems.size();
}

void DebugLineRenderQueue::Clear()
{
	//std::lock_guard<std::mutex> lock(m_Mutex);
	m_DrawItems.clear();
}
