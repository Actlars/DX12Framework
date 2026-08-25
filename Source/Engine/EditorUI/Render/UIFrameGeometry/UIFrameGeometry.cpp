#include "UIFrameGeometry.h"

// -------------------------------------------------------------------------------
// 複数WindowのDrawListを１つの頂点・IndexBufferへまとめてGPUへアップロード
// -------------------------------------------------------------------------------
const std::vector<UIFlatDrawCommand>& UIGeometry::Upload(
    const std::vector<const EditorUI::DrawList*>& _pWindowDrawLists, 
    ID3D12Device* _pDevice)
{
    // 前フレームの描画コマンドを破棄
	m_FlatCommands.clear();

    // 合計サイズを集計する。
    // WindowごとにGPUBufferの容量確認・再確保を行うのを防ぐ
    size_t totalVertexCount = 0;
    size_t totalIndexCount  = 0;
    for (const EditorUI::DrawList* dl : _pWindowDrawLists)
    {
        totalVertexCount    += dl->GetVertices().size();
        totalIndexCount     += dl->GetIndices().size();
    }

    // 描画する頂点またはIndexがなければ描画しない
    if (totalVertexCount == 0 || totalIndexCount == 0)
    {
        return m_FlatCommands;
    }

    // 必要な頂点・Index数を格納できるGPUBuffer容量を確保する
    EnsureCapacity(totalVertexCount,totalIndexCount,_pDevice);

    // -------------------------------------------------------------------------------
    // CPU側でいったんマージしてから最後にまとめてGPUへ書き込む
    // -------------------------------------------------------------------------------

    // 全Window分の頂点・Indexを一度CPU側でまとめて最後にGPUBufferへ一括で書き込む
    std::vector<EditorUI::UIVertex> mergedVertices;
    std::vector<uint32_t> mergedIndices;
    // 必要サイズがわかっているため、先にメモリを確保してvectorの再確保を防ぐ
    mergedVertices.reserve(totalVertexCount);
    mergedIndices.reserve(totalIndexCount);

    // 次のDrawListの頂点Indexに加算する基準値
    uint32_t vertexBase = 0;
    // 次のDrawListのDrawCommandに加算するIndexBufferの開始位置
    uint32_t indexBase  = 0;

    for (const EditorUI::DrawList* dl : _pWindowDrawLists)
    {
        // DrawListから頂点とインデックスを取得
        const auto& vertices    = dl->GetVertices();
        const auto& indices     = dl->GetIndices();
        
        // -------------------------------------------------------------------------------
        // 頂点を結合
        // -------------------------------------------------------------------------------
        
        // 頂点はそのまま後ろへ追加できる
        mergedVertices.insert(mergedVertices.end(), vertices.begin(), vertices.end());

        // -------------------------------------------------------------------------------
        // Indexを結合
        // -------------------------------------------------------------------------------

        // 各DrawList内のIndexは、そのDrawListの頂点配列を基準としている
        for (uint32_t index : indices)
        {
            mergedIndices.push_back(vertexBase + index);
        }

        // -------------------------------------------------------------------------------
        // DrawCommandをFlatCommandへ変換
        // -------------------------------------------------------------------------------

        for (const EditorUI::DrawCommand& cmd : dl->GetCommands())
        {
            // 各DrawListのIndexOffsetも、そのDrawList内を基準としているため、
            // これまでに追加されたIndex数を加算する
            m_FlatCommands.push_back({ cmd.IndexOffset + indexBase, cmd.ElementCount, cmd.ClipRect,cmd.Texture });
        }

        // 次の頂点、インデックスに進む
        vertexBase  += static_cast<uint32_t>(vertices.size());
        indexBase   += static_cast<uint32_t>(indices.size());
    }

    // -------------------------------------------------------------------------------
    // 今フレームで実際に使用するBufferサイズを記録
    // -------------------------------------------------------------------------------

    // 使用しているメモリのサイズ
    m_VertexUsedBytes   = mergedVertices.size() * sizeof(EditorUI::UIVertex);
    m_IndexUsedBytes    = mergedIndices.size() * sizeof(uint32_t);

    // -------------------------------------------------------------------------------
    // GPUBufferへデータを書き込み
    // -------------------------------------------------------------------------------

    // メモリを書き込む
    m_VertexBuffer.Write(mergedVertices.data(), m_VertexUsedBytes);
    m_IndexBuffer.Write(mergedIndices.data(), m_IndexUsedBytes);

    // Rendererが順番に描画するためのコマンド一覧を返す
    return m_FlatCommands;
}

// -------------------------------------------------------------------------------
// 頂点BufferViewを取得
// -------------------------------------------------------------------------------
D3D12_VERTEX_BUFFER_VIEW UIGeometry::GetVertexBufferView() const
{
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    // 頂点BufferのGPU仮想アドレス
    vbv.BufferLocation  = m_VertexBuffer.GetAddress();
    // 今フレームで実際に使用している頂点データのサイズ
    vbv.SizeInBytes     = static_cast<UINT>(m_VertexUsedBytes);
    // 1頂点あたりのサイズ
    vbv.StrideInBytes   = sizeof(EditorUI::UIVertex);
    return vbv;
}

// -------------------------------------------------------------------------------
// IndexBufferViewを取得
// -------------------------------------------------------------------------------
D3D12_INDEX_BUFFER_VIEW UIGeometry::GetIndexBufferView() const
{
    D3D12_INDEX_BUFFER_VIEW ibv{};
    // 必要な頂点BufferサイズをByte単位で計算
    ibv.BufferLocation  = m_IndexBuffer.GetAddress();
    // 必要なIndexBufferサイズをByte単位で計算
    ibv.SizeInBytes     = static_cast<UINT>(m_IndexUsedBytes);
    // Indexはuint32_tを使用しているため32bit形式
    ibv.Format          = DXGI_FORMAT_R32_UINT;
    return ibv;
}

// -------------------------------------------------------------------------------
// 頂点・IndexBufferの容量を確保
// -------------------------------------------------------------------------------
void UIGeometry::EnsureCapacity(size_t _vertexCount, size_t _indexCount, ID3D12Device* _pDevice)
{
    // 必要な頂点BufferサイズをByte単位で計算
    const size_t requiredVertexBytes    = _vertexCount * sizeof(EditorUI::UIVertex);
    // 必要なIndexBufferサイズをByte単位で計算
    const size_t requiredIndexBytes     = _indexCount * sizeof(uint32_t);

    // -------------------------------------------------------------------------------
    // 頂点Bufferの容量確認
    // -------------------------------------------------------------------------------

    // 容量が足りない時だけ再確保する
    // 頂点の増減に応じて毎フレーム再確保するのを防ぐため、少しだけ余裕を持たせてメモリを確保する
    if (requiredVertexBytes >= m_VertexCapacityBytes)
    {
        // 古いBufferを破棄
        m_VertexBuffer.Term();
        // 必要なサイズに少し余裕を加えた容量を確保
        const size_t newCapacity = requiredVertexBytes + requiredVertexBytes / LeaveSomeLeeway;
        RHI::BufferDesc desc{ newCapacity,RHI::BufferHeapType::Upload, false };
        m_VertexBuffer.Init(_pDevice, desc);
        m_VertexCapacityBytes = newCapacity;
    }

    // -------------------------------------------------------------------------------
    // IndexBufferの容量確認
    // -------------------------------------------------------------------------------
    if (requiredIndexBytes >= m_IndexCapacityBytes)
    {
        // 古いBufferを破棄
        m_IndexBuffer.Term();
        // 必要なサイズに少し余裕を加えた容量を確保
        const size_t newCapacity = requiredIndexBytes + requiredIndexBytes / LeaveSomeLeeway;
        RHI::BufferDesc desc{ newCapacity, RHI::BufferHeapType::Upload, false };
        m_IndexBuffer.Init(_pDevice, desc);
        m_IndexCapacityBytes = newCapacity;
    }
}
