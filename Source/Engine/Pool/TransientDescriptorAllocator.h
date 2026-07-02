#pragma once
#include <Engine/Utility/ComPtr/ComPtr.h>
#include <Engine/Pool/DescriptorPool/DescriptorPool.h> // DescriptorHandle定義の再利用

// -------------------------------------------------------------------------------
// TransientDescriptorAllocator
// 
// 概要 :
//	フレームごとに大量発生する一時的なディスクリプタ（動的SRV/UAV等）を
//	リングバッファ方式で確保する
// 
//	DescriptorPool（永続型・FreeList）とは異なり、個別のFree()は行わない
//	フレーム末にReset()を呼ぶことで、次フレームから領域を丸ごと再利用する
// 
//	用途 :
//		- GPUパーティクルシステムの一時UAV/SRV
//		- フレームごとに動的生成されるディスクリプタテーブル
// -------------------------------------------------------------------------------
class TransientDescriptorAllocator
{
public:
	// -------------------------------------------------------------------------------
	// @brief	初期化
	// 
	// @param[in]	_pDevice	デバイス
	// @param[in]	_type		ディスクリプタヒープタイプ
	// @param[in]	_capacity	1フレームあたりの最大確保数
	// -------------------------------------------------------------------------------
	bool Init(ID3D12Device* _pDevice, D3D12_DESCRIPTOR_HEAP_TYPE _type, uint32_t _capacity)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = _type;
		desc.NumDescriptors = _capacity;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		desc.NodeMask = 0;

		auto hr = _pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_pHeap.GetAddressOf()));
		if (FAILED(hr)) { return false; }

		m_DescriptorSize = _pDevice->GetDescriptorHandleIncrementSize(_type);
		m_Capacity = _capacity;
		m_CurrentOffset = 0;

		return true;
	}

	// -------------------------------------------------------------------------------
	// @brief	一時ディスクリプタを_count個連続で確保する
	//			確保に失敗した場合は容量オーバーなので、この時点でcapacityを見直すべき
	// -------------------------------------------------------------------------------
	DescriptorHandle Allocate(uint32_t _count = 1)
	{
		DescriptorHandle handle = {};

		// 容量を超える場合は先頭に巻き戻す（本来は1フレームで超えないよう設計する）
		if (m_CurrentOffset + _count > m_Capacity)
		{
			//ELOG("TransientDescriptorAllocator::Allocate() capacity exceeded, wrapping (data may be overwritten)");
			m_CurrentOffset = 0;
		}

		auto cpuStart = m_pHeap->GetCPUDescriptorHandleForHeapStart();
		auto gpuStart = m_pHeap->GetGPUDescriptorHandleForHeapStart();

		handle.HandleCPU.ptr = cpuStart.ptr + m_DescriptorSize * m_CurrentOffset;
		handle.HandleGPU.ptr = gpuStart.ptr + m_DescriptorSize * m_CurrentOffset;

		m_CurrentOffset += _count;
		return handle;
	}

	// -------------------------------------------------------------------------------
	// @brief	確保位置をリセットする。フレーム末（EndFrame等）で呼ぶこと
	// -------------------------------------------------------------------------------
	void Reset()
	{
		m_CurrentOffset = 0;
	}

	ID3D12DescriptorHeap* GetHeap() const { return m_pHeap.Get(); }

private:
	ComPtr<ID3D12DescriptorHeap> m_pHeap;
	uint32_t m_DescriptorSize = 0;
	uint32_t m_Capacity = 0;
	uint32_t m_CurrentOffset = 0;
};