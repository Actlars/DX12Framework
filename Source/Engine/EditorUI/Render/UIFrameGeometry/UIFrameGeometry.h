#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Types.h>
#include <Engine/EditorUI/Core/DrawList.h>
#include <Engine/RHI/Resource/Buffer/Buffer.h>

// -------------------------------------------------------------------------------
// UIFlatDrawCommand struct
// 
// 概要 : 
//	マージ後の1コマンド分
// -------------------------------------------------------------------------------
struct UIFlatDrawCommand
{
	uint32_t				IndexOffset		= 0;	// 開始地点のオフセット
	uint32_t				ElementCount	= 0;	// 要素数
	EditorUI::Rect2D		ClipRect		= {};	// UIのサイズ
	EditorUI::TextureId		Texture			= 0;	// GPU上のどのテクスチャを使うかのId
};

// -------------------------------------------------------------------------------
// UIGeometry class
// 
// 概要 : 
//	WindowDrawListsを１本の頂点/インデックス配列にマージしてGPUへアップロードする
// -------------------------------------------------------------------------------
class UIGeometry
{
public:

	const std::vector<UIFlatDrawCommand>& Upload(
		const std::vector<const EditorUI::DrawList*>&	_pWindowDrawLists,
		ID3D12Device*									_pDevice);

	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const;
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const;

private:

	void EnsureCapacity(size_t _vertexCount, size_t _indexCount, ID3D12Device* _pDevice);

	RHI::Buffer m_VertexBuffer;						// Upload
	RHI::Buffer m_IndexBuffer;						// Uplaod

	size_t m_VertexCapacityBytes	= 0;	// 現在確保済みの容量
	size_t m_IndexCapacityBytes		= 0;	
	size_t m_VertexUsedBytes		= 0;	// 直近のUploadで実際に書き込んだサイズ（Viewに使用）
	size_t m_IndexUsedBytes			= 0;

	std::vector<UIFlatDrawCommand> m_FlatCommands;	// Upload()実行結果のキャッシュ

	size_t LeaveSomeLeeway = 2;	// メモリ確保時の余裕（ちょうどのサイズ / 2）
};

