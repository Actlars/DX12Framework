// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "CommandList.h"
#include <Engine/Fence/Fence.h>

// -------------------------------------------------------------------------------
// CommandList class
// -------------------------------------------------------------------------------

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
CommandList::CommandList()
: m_pCmdList(nullptr)
, m_pAllocators()
, m_FenceValues()
, m_Index(0)
{ /* DO_NOTHING */ }


// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
CommandList::~CommandList() 
{ Term(); }

// -------------------------------------------------------------------------------
//		初期化処理
// -------------------------------------------------------------------------------
bool CommandList::Init(ID3D12Device* _pDevice, D3D12_COMMAND_LIST_TYPE _type, uint32_t _count)
{
	if (_pDevice == nullptr || _count == 0)
	{ return false; }

	m_pAllocators.resize(_count);
	m_FenceValues.assign(_count, 0);	// まだGPUに送信していないので、完了フェンス値は0で初期化

	for (auto i = 0u; i < _count; ++i)
	{
		auto hr = _pDevice->CreateCommandAllocator(
			_type, IID_PPV_ARGS(m_pAllocators[i].GetAddressOf()));
		if (FAILED(hr)) 
		{ return false; }
	}

	{
		auto hr = _pDevice->CreateCommandList(
			1,
			_type,
			m_pAllocators[0].Get(),
			nullptr,
			IID_PPV_ARGS(m_pCmdList.GetAddressOf()));
		if (FAILED(hr)) 
		{ return false; }

		m_pCmdList->Close();
	}

	m_Index = 0;
	return true;
}

// -------------------------------------------------------------------------------
//		終了処理
// -------------------------------------------------------------------------------
void CommandList::Term()
{
	m_pCmdList.Reset();

	for (size_t i = 0; i < m_pAllocators.size(); ++i) 
	{ m_pAllocators[i].Reset(); }

	m_pAllocators.clear();
	m_pAllocators.shrink_to_fit();

	m_FenceValues.clear();
	m_FenceValues.shrink_to_fit();
}

// -------------------------------------------------------------------------------
//		リセット処理（FenceでGPU使用中か確認してからリセット）
// -------------------------------------------------------------------------------
ID3D12GraphicsCommandList* CommandList::Reset(Fence* _pFence)
{
	// このアロケータが前回使われたときの完了予定値が
	// GPUの完了値より大きい場合は、まだGPUが使っているので待機する
	if (_pFence != nullptr) 
	{ _pFence->WaitForValue(m_FenceValues[m_Index]); }

	// コマンドアロケータをリセットする
	auto hr = m_pAllocators[m_Index]->Reset();
	if (FAILED(hr)) 
	{ return nullptr; }

	// コマンドリストをリセットする
	hr = m_pCmdList->Reset(m_pAllocators[m_Index].Get(), nullptr);
	if (FAILED(hr)) 
	{ return nullptr; }

	// ここではインデックスを進めない
	// 「今フレームがどのアロケータを使っているか」はRecordFenceValueが呼ばれるまで確定させる
	return m_pCmdList.Get();
}

void CommandList::RecordFenceValue(UINT64 _value)
{
	// 今使ったアロケータの完了フェンス値を記録する
	m_FenceValues[m_Index] = _value;
	// 次のアロケータに切り替える
	m_Index = (m_Index + 1) % uint32_t(m_pAllocators.size());
}
