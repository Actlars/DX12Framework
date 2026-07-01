// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Fence.h"

// -------------------------------------------------------------------------------
// Fence class
// -------------------------------------------------------------------------------

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
Fence::Fence()
:m_pFence(nullptr)
,m_Event(nullptr)
,m_Counter(0)
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
Fence::~Fence() 
{ Term(); }

// -------------------------------------------------------------------------------
//		初期化処理を行う
// -------------------------------------------------------------------------------
bool Fence::Init(ID3D12Device* _pDevice)
{
	if (_pDevice == nullptr) 
	{ return false; }

	// イベントを生成
	m_Event = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
	if (m_Event == nullptr) 
	{ return false; }

	// フェンスを生成
	auto hr = _pDevice->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(m_pFence.GetAddressOf()));
	if (FAILED(hr)) 
	{ return false; }

	// カウンタ設定
	m_Counter = 1;

	// 正常終了
	return true;
}

// -------------------------------------------------------------------------------
//		終了処理を行う
// -------------------------------------------------------------------------------
void Fence::Term()
{
	// ハンドルを閉じる
	if (m_Event != nullptr)
	{
		CloseHandle(m_Event);
		m_Event = nullptr;
	}

	// フェンスオブジェクトを破棄
	m_pFence.Reset();

	// カウンターリセット
	m_Counter = 0;
}

// -------------------------------------------------------------------------------
//		シグナルを発行し、完了フェンス値を返す
// -------------------------------------------------------------------------------
UINT64 Fence::Signal(ID3D12CommandQueue* _pQueue)
{
	if (_pQueue == nullptr) 
	{ return 0; }

	// 現在のカウンター値を取得してシグナルを発行
	const auto value = m_Counter;
	// シグナル処理（GPUにシグナルを送る）
	auto hr = _pQueue->Signal(m_pFence.Get(), value);
	if (FAILED(hr)) 
	{ return 0; }

	// カウンターを増やす
	m_Counter++;

	return m_Counter;
}

// -------------------------------------------------------------------------------
//		GPUで指定したフェンス値が完了するまで待機する
// -------------------------------------------------------------------------------
void Fence::WaitForValue(UINT64 _value)
{
	// すでに完了していれば即座に戻る
	if (m_pFence->GetCompletedValue() < _value)
	{
		// 完了時にイベントを設定
		auto hr = m_pFence->SetEventOnCompletion(_value, m_Event);
		if (FAILED(hr)) 
		{ return; }

		// 待機処理
		WaitForSingleObjectEx(m_Event, INFINITE, FALSE);
	}
}



//// -------------------------------------------------------------------------------
////		シグナル状態になるまで指定時間待機
//// -------------------------------------------------------------------------------
//void Fence::Wait(ID3D12CommandQueue* _pQueue, UINT _timeout)
//{
//	if (_pQueue == nullptr) 
//	{ return; }
//
//	const auto fenceValue = m_Counter;
//
//	// シグナル処理
//	auto hr = _pQueue->Signal(m_pFence.Get(), fenceValue);
//	if (FAILED(hr)) 
//	{ return; }
//
//	// カウンターを増やす
//	m_Counter++;
//
//	// 次のフレームの描画準備がまだであれば待機
//	if (m_pFence->GetCompletedValue() < fenceValue)
//	{
//		// 完了時にイベントを設定
//		auto hr = m_pFence->SetEventOnCompletion(fenceValue, m_Event);
//		if (FAILED(hr)) 
//		{ return; }
//
//		// 待機処理
//		if (WAIT_OBJECT_0 != WaitForSingleObjectEx(m_Event, _timeout, FALSE)) 
//		{ return; }
//	}
//}

// -------------------------------------------------------------------------------
//		シグナル状態になるまでずっと待機
// -------------------------------------------------------------------------------
void Fence::Sync(ID3D12CommandQueue* _pQueue)
{
	if (_pQueue == nullptr) 
	{ return; }

	// シグナル処理
	auto hr = _pQueue->Signal(m_pFence.Get(), m_Counter);
	if (FAILED(hr)) 
	{ return; }

	// 完了時にイベントを設定
	hr = m_pFence->SetEventOnCompletion(m_Counter, m_Event);
	if (FAILED(hr)) 
	{ return; }

	// 待機処理
	if (WAIT_OBJECT_0 != WaitForSingleObjectEx(m_Event, INFINITE, FALSE)) 
	{ return; }

	// カウンターを増やす
	m_Counter++;
}
