// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "DescriptorPool.h"

// -------------------------------------------------------------------------------
// DescriptorPool class
// -------------------------------------------------------------------------------


// -------------------------------------------------------------------------------
//	コンストラクタ
// -------------------------------------------------------------------------------
DescriptorPool::DescriptorPool()
: m_RefCount (1)
, m_Pool ()
, m_pHeap()
, m_DescriptorSize(0)
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//	デストラクタ
// -------------------------------------------------------------------------------
DescriptorPool::~DescriptorPool()
{
	m_Pool.Term();
	m_pHeap.Reset();
	m_DescriptorSize = 0;
}

// -------------------------------------------------------------------------------
//	参照カウントを増やす
// -------------------------------------------------------------------------------
void DescriptorPool::AddRef()
{
	m_RefCount++;
}

// -------------------------------------------------------------------------------
//	解放処理を行う
// -------------------------------------------------------------------------------
void DescriptorPool::Release()
{
	m_RefCount--;
	if (m_RefCount == 0)
	{
		delete this;
	}
}

// -------------------------------------------------------------------------------
//	参照カウントを取得
// -------------------------------------------------------------------------------
uint32_t DescriptorPool::GetCount() const
{
	return m_RefCount;
}

// -------------------------------------------------------------------------------
//	ディスクリプタハンドルを割り当て
// -------------------------------------------------------------------------------
DescriptorHandle* DescriptorPool::AllocHandle()
{
	// 初期化関数
	auto func = [&](uint32_t index, DescriptorHandle* pHandle)
		{
			auto handleCPU = m_pHeap->GetCPUDescriptorHandleForHeapStart();
			handleCPU.ptr += m_DescriptorSize * index;

			auto handleGPU = m_pHeap->GetGPUDescriptorHandleForHeapStart();
			handleGPU.ptr += m_DescriptorSize * index;

			pHandle->HandleCPU = handleCPU;
			pHandle->HandleGPU = handleGPU;
		};

	// 初期化関数を実行してからハンドルを返却
	return m_Pool.Alloc(func);
}

// -------------------------------------------------------------------------------
//	ディスクリプタハンドルを解放
// -------------------------------------------------------------------------------
void DescriptorPool::FreeHandle(DescriptorHandle*& _pHandle)
{
	if (_pHandle != nullptr)
	{
		// ハンドルをプールに戻す
		m_Pool.Free(_pHandle);

		// nullptrでクリア
		_pHandle = nullptr;
	}
}

// -------------------------------------------------------------------------------
//	利用可能なハンドル数を取得
// -------------------------------------------------------------------------------
uint32_t DescriptorPool::GetAvailableHandleCount() const
{
	return m_Pool.GetAvailableCount();
}

// -------------------------------------------------------------------------------
//	割り当て済みのハンドル数を取得
// -------------------------------------------------------------------------------
uint32_t DescriptorPool::GetAllocatedHandleCount() const
{
	return m_Pool.GetUsedCount();
}

// -------------------------------------------------------------------------------
//	ハンドル総数を取得
// -------------------------------------------------------------------------------
uint32_t DescriptorPool::GetHandleCount() const
{
	return m_Pool.GetSize();
}

// -------------------------------------------------------------------------------
//	ディスクリプタヒープを取得
// -------------------------------------------------------------------------------
ID3D12DescriptorHeap* const DescriptorPool::GetHeap() const
{
	return m_pHeap.Get();
}

// -------------------------------------------------------------------------------
//	生成処理を行う
// -------------------------------------------------------------------------------
bool DescriptorPool::Create
(
	ID3D12Device*						_pDevice,
	const D3D12_DESCRIPTOR_HEAP_DESC *	_pDesc,
	DescriptorPool**					_ppPool
)
{
	// 引数チェック
	if (_pDevice == nullptr || _pDesc == nullptr || _ppPool == nullptr) 
	{ return false; }

	// インスタンスを生成
	auto instance = new(std::nothrow) DescriptorPool();
	if (instance == nullptr) 
	{ return false; }

	// ディスクリプタヒープを生成
	auto hr = _pDevice->CreateDescriptorHeap(
		_pDesc,
		IID_PPV_ARGS(instance->m_pHeap.GetAddressOf()));

	// 失敗したら解放処理を行って終了
	if (FAILED(hr))
	{
		instance->Release();
		return false;
	}

	// プールを初期化
	if (!instance->m_Pool.Init(_pDesc->NumDescriptors))
	{
		instance->Release();
		return false;
	}

	// ディスクリプタの加算サイズを取得
	instance->m_DescriptorSize =
		_pDevice->GetDescriptorHandleIncrementSize(_pDesc->Type);

	// インスタンスを設定
	*_ppPool = instance;

	// 正常終了
	return true;
}
