// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "DebugLineResource.h"
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
//		コンストラクタ	
// -------------------------------------------------------------------------------
DebugLineResource::DebugLineResource()
{ /* DO_NOTHING */
}

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
DebugLineResource::~DebugLineResource()
{
	Term();
}

// -------------------------------------------------------------------------------
// 初期化
// -------------------------------------------------------------------------------
bool DebugLineResource::Init(ID3D12Device* _pDevice, uint32_t _frameCount, uint32_t _maxVertexCount)
{
	if (_pDevice == nullptr || _maxVertexCount == 0) 
	{ return false; }

	Term();

	m_MaxVertexCount = _maxVertexCount;
	m_FrameBuffers.resize(_frameCount);

	for (auto& frameBuffer : m_FrameBuffers)
	{
		// 頂点バッファの生成
		if (!CreateVertexBuffer(_pDevice, frameBuffer))
		{
			ELOG("DebugLineResource::Init() CreateVertexBuffer() failed");
			return false;
		}
	}

	return true;
}

// -------------------------------------------------------------------------------
//		終了処理
// -------------------------------------------------------------------------------
void DebugLineResource::Term()
{
	for (auto& frameBuffer : m_FrameBuffers)
	{
		if (frameBuffer.Buffer != nullptr &&
			frameBuffer.pMappedData != nullptr)
		{
			frameBuffer.Buffer->Unmap(0, nullptr);
		}
		frameBuffer.pMappedData = nullptr;
		frameBuffer.Buffer.Reset();
		frameBuffer.VBV = {};
		frameBuffer.VertexCount = 0;
	}
	m_FrameBuffers.clear();
	m_MaxVertexCount = 0;
}

// -------------------------------------------------------------------------------
// 頂点データ更新
// -------------------------------------------------------------------------------
bool DebugLineResource::UpdateVertexBuffer(uint32_t _frameIndex, const DebugLineVertex* _pVertices, uint32_t _vertexCount)
{
	if (_frameIndex >= m_FrameBuffers.size())
	{
		ELOG("DebugLineResource::UpdateVertexBuffer() invalid frame Index");
		return false;
	}

	auto& frameBuffer = m_FrameBuffers[_frameIndex];
	// 描画する頂点がない
	if (_vertexCount == 0)
	{
		frameBuffer.VertexCount = 0;
		return true;
	}

	if (_pVertices == nullptr || frameBuffer.pMappedData == nullptr)
	{
		frameBuffer.VertexCount = 0;
		return false;
	}

	if (_vertexCount > m_MaxVertexCount)
	{
		ELOG("DebugLineResource::UpdateVertexBuffer() VertexCount exceeds MaxVertexCount");
		return false;
	}

	// -------------------------------------------------------------------------------
	// CPU側で作成したDebugLine用頂点データをGPU側にコピー
	// -------------------------------------------------------------------------------
	const size_t dataSize = sizeof(DebugLineVertex) * _vertexCount;
	memcpy(frameBuffer.pMappedData, _pVertices, dataSize);
	frameBuffer.VertexCount = _vertexCount;

	return true;
}

// -------------------------------------------------------------------------------
// 描画コマンドを積む
// -------------------------------------------------------------------------------
void DebugLineResource::Draw(ID3D12GraphicsCommandList* _pCmd, uint32_t _frameIndex)
{
	if (_pCmd == nullptr || _frameIndex >= m_FrameBuffers.size()) 
	{ return; }

	const auto& frameBuffer = m_FrameBuffers[_frameIndex];

	if (frameBuffer.VertexCount == 0)
	{ return; }

	// -------------------------------------------------------------------------------
	// IA(InputAssembly)に頂点バッファをセット
	// -------------------------------------------------------------------------------
	_pCmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	_pCmd->IASetVertexBuffers(0, 1, &frameBuffer.VBV);

	// -------------------------------------------------------------------------------
	// ライン描画
	// -------------------------------------------------------------------------------
	_pCmd->DrawInstanced(frameBuffer.VertexCount, 1, 0, 0);
}

// -------------------------------------------------------------------------------
// 頂点バッファ生成
// -------------------------------------------------------------------------------
bool DebugLineResource::CreateVertexBuffer(ID3D12Device* _pDevice, FrameVertexBuffer& _frameBuffer)
{
	// -------------------------------------------------------------------------------
	// DebugLineは毎フレーム頂点データが変化するため、
	// CPUから直接更新できるUPLOADヒープを使用
	// -------------------------------------------------------------------------------
	const size_t bufferSize = sizeof(DebugLineVertex) * m_MaxVertexCount;

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width				= static_cast<UINT64>(bufferSize);
	desc.Height				= 1;
	desc.DepthOrArraySize	= 1;
	desc.MipLevels			= 1;
	desc.Format				= DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count	= 1;
	desc.Layout				= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags				= D3D12_RESOURCE_FLAG_NONE;

	auto hr = _pDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_frameBuffer.Buffer.GetAddressOf()));
	if (FAILED(hr))
	{
		return false;
	}

	// -------------------------------------------------------------------------------
	// 頂点バッファービュー
	// -------------------------------------------------------------------------------
	_frameBuffer.VBV.BufferLocation	= _frameBuffer.Buffer->GetGPUVirtualAddress();
	_frameBuffer.VBV.SizeInBytes	= static_cast<UINT>(bufferSize);
	_frameBuffer.VBV.StrideInBytes	= static_cast<UINT>(sizeof(DebugLineVertex));

	// -------------------------------------------------------------------------------
	// DebugLineは毎フレーム更新するため、Map状態を維持する
	// UPLOADヒープは常にMapしていても問題ない
	// -------------------------------------------------------------------------------
	D3D12_RANGE readRange = { 0,0 };

	if (FAILED(_frameBuffer.Buffer->Map(0, &readRange, reinterpret_cast<void**>(&_frameBuffer.pMappedData))))
	{
		_frameBuffer.Buffer.Reset();
		return false;
	}

	return true;
}
