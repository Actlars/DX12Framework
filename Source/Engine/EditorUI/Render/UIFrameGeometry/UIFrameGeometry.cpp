#include "UIFrameGeometry.h"

const std::vector<UIFlatDrawCommand>& UIGeometry::Upload(
    const std::vector<const EditorUI::DrawList*>& _pWindowDrawLists, 
    ID3D12Device* _pDevice)
{
	m_FlatCommands.clear();

    // 合計サイズを集計する。EnsureCapacityで一括判定するため、
    // ウィンドウごとに逐次拡張するのではなく先にトータルを出す
    size_t totalVertexCount = 0;
    size_t totalIndexCount  = 0;
    for (const EditorUI::DrawList* dl : _pWindowDrawLists)
    {
        totalVertexCount    += dl->GetVertices().size();
        totalIndexCount     += dl->GetIndices().size();
    }

    if (totalVertexCount == 0 || totalIndexCount == 0)
    {
        return m_FlatCommands;  // 描画するものがない
    }

    EnsureCapacity(totalVertexCount,totalIndexCount,_pDevice);

    // CPU側でいったんマージしてから最後にまとめてGPUへ書き込む
    std::vector<EditorUI::UIVertex> mergedVertices;
    std::vector<uint32_t> mergedIndices;
    mergedVertices.reserve(totalVertexCount);
    mergedIndices.reserve(totalIndexCount);

    uint32_t vertexBase = 0;
    uint32_t indexBase  = 0;

    for (const EditorUI::DrawList* dl : _pWindowDrawLists)
    {
        // DrawListから頂点とインデックスを取得
        const auto& vertices    = dl->GetVertices();
        const auto& indices     = dl->GetIndices();
        
        // 頂点はそのままコピーするだけでよい
        mergedVertices.insert(mergedVertices.end(), vertices.begin(), vertices.end());

        // インデックスは元の値にvertexBaseを足しながらコピーする
        for (uint32_t index : indices)
        {
            mergedIndices.push_back(vertexBase + index);
        }

        // m_FlatCommandsにcmdのindexだけindexBaseを足したものを詰め込む
        for (const EditorUI::DrawCommand& cmd : dl->GetCommands())
        {
            m_FlatCommands.push_back({ cmd.IndexOffset + indexBase, cmd.ElementCount, cmd.ClipRect,cmd.Texture });
        }

        // 次の頂点、インデックスに進む
        vertexBase  += static_cast<uint32_t>(vertices.size());
        indexBase   += static_cast<uint32_t>(indices.size());
    }

    // 使用しているメモリのサイズ
    m_VertexUsedBytes   = mergedVertices.size() * sizeof(EditorUI::UIVertex);
    m_IndexUsedBytes    = mergedIndices.size() * sizeof(uint32_t);

    // メモリを書き込む
    m_VertexBuffer.Write(mergedVertices.data(), m_VertexUsedBytes);
    m_IndexBuffer.Write(mergedIndices.data(), m_IndexUsedBytes);

    return m_FlatCommands;
}

D3D12_VERTEX_BUFFER_VIEW UIGeometry::GetVertexBufferView() const
{
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    vbv.BufferLocation  = m_VertexBuffer.GetAddress();
    vbv.SizeInBytes     = static_cast<UINT>(m_VertexUsedBytes);
    vbv.StrideInBytes   = sizeof(EditorUI::UIVertex);
    return vbv;
}

D3D12_INDEX_BUFFER_VIEW UIGeometry::GetIndexBufferView() const
{
    D3D12_INDEX_BUFFER_VIEW ibv{};
    ibv.BufferLocation  = m_IndexBuffer.GetAddress();
    ibv.SizeInBytes     = static_cast<UINT>(m_IndexUsedBytes);
    ibv.Format          = DXGI_FORMAT_R32_UINT;
    return ibv;
}

void UIGeometry::EnsureCapacity(size_t _vertexCount, size_t _indexCount, ID3D12Device* _pDevice)
{
    const size_t requiredVertexBytes    = _vertexCount * sizeof(EditorUI::UIVertex);
    const size_t requiredIndexBytes     = _indexCount * sizeof(uint32_t);

    // 容量が足りない時だけ再確保する
    // 頂点の増減に応じて毎フレーム再確保するのを防ぐため、少しだけ余裕を持たせてメモリを確保する
    if (requiredVertexBytes >= m_VertexCapacityBytes)
    {
        m_VertexBuffer.Term();
        const size_t newCapacity = requiredVertexBytes + requiredVertexBytes / LeaveSomeLeeway;
        RHI::BufferDesc desc{ newCapacity,RHI::BufferHeapType::Upload, false };
        m_VertexBuffer.Init(_pDevice, desc);
        m_VertexCapacityBytes = newCapacity;
    }

    if (requiredIndexBytes >= m_IndexCapacityBytes)
    {
        m_IndexBuffer.Term();
        const size_t newCapacity = requiredIndexBytes + requiredIndexBytes / LeaveSomeLeeway;
        RHI::BufferDesc desc{ newCapacity, RHI::BufferHeapType::Upload, false };
        m_IndexBuffer.Init(_pDevice, desc);
        m_IndexCapacityBytes = newCapacity;
    }
}
