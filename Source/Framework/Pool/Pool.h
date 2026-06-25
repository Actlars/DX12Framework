#pragma once

// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------

// -------------------------------------------------------------------------------
// Pool class
// -------------------------------------------------------------------------------
template<typename T>
class Pool
{
	// -------------------------------------------------------------------------------
	// list of friend classes and methods
	// -------------------------------------------------------------------------------
	/* NOTHING */

public:

	// -------------------------------------------------------------------------------
	// public variables
	// -------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------
	// コンストラクタ
	// -------------------------------------------------------------------------------
	Pool()
		: m_pBuffer(nullptr)
		, m_pActive(nullptr)
		, m_pFree(nullptr)
		, m_Capacity(0)
		, m_Count(0)
	{ /* DO_NOTHING */ }

	// -------------------------------------------------------------------------------
	// デストラクタ
	// -------------------------------------------------------------------------------
	~Pool() 
	{ Term(); }

	// -------------------------------------------------------------------------------
	// @brief	初期化を行う
	// 
	// @param[in]	count	確保するアイテム数
	// @retval	true	初期化に成功
	// @retval	false	初期化に失敗
	// -------------------------------------------------------------------------------
	bool Init(uint32_t _count)
	{
		std::lock_guard<std::mutex> guard(m_Mutex);

		// Itemの数分 * 2 のメモリを確保
		m_pBuffer = static_cast<uint8_t*>(malloc(sizeof(Item) * (_count * 2)));
		if (m_pBuffer == nullptr) 
		{ return false; }

		m_Capacity = _count;
		
		// インデックスを振る
		for (auto i = 2u, j = 0u; i < m_Capacity + 2; ++i, ++j)
		{
			auto item = GetItem(i);
			item->m_Index = j;
		}

		// アクティブアイテムの先頭のインデックスの初期化
		m_pActive = GetItem(0);									// 先頭のItemを取得
		m_pActive->m_pPrev = m_pActive->m_pNext = m_pActive;	// 前と次のアイテムへのポインタを自分に設定
		m_pActive->m_Index = uint32_t(-1);						// 先頭アイテムのインデックスを－1に設定

		m_pFree = GetItem(1);					// フリーアイテムの先頭を取得
		m_pFree->m_Index = uint32_t(-2);		// フリーアイテムのインデックスを－２に設定

		// 1番目のItemから前のItemへのポインタをnullptr、次のポインタをアドレスの配列から取得
		for (auto i = 1u; i < m_Capacity + 2; ++i)
		{
			GetItem(i)->m_pPrev = nullptr;
			GetItem(i)->m_pNext = GetItem(i + 1);
		}

		// 最後のItemの前のアイテムへのポインタをフリーアイテムの先頭に接続
		GetItem(m_Capacity + 1)->m_pPrev = m_pFree;

		// Itemの数を初期化
		m_Count = 0;

		return true;
	}

	// -------------------------------------------------------------------------------
	// @brief	終了処理
	// -------------------------------------------------------------------------------
	void Term()
	{
		std::lock_guard<std::mutex> guard(m_Mutex);

		if (m_pBuffer)
		{
			free(m_pBuffer);
			m_pBuffer = nullptr;
		}

		m_pActive	= nullptr;
		m_pFree		= nullptr;
		m_Capacity	= 0;
		m_Count		= 0;
	}

	// -------------------------------------------------------------------------------
	// @brief	アイテムを確保
	// 
	// @param[in]	func	ユーザによる初期化処理
	// @return	確保したアイテムへのポインタ.確保に失敗した場合はnullptrが返却される
	// -------------------------------------------------------------------------------
	T* Alloc(std::function<void(uint32_t, T*)> _func = nullptr)
	{
		std::lock_guard<std::mutex> guard(m_Mutex);

		// 確保失敗
		if (m_pFree->m_pNext == m_pFree || m_Count + 1 > m_Capacity) 
		{ return nullptr; }

		auto item			= m_pFree->m_pNext;	// フリーアイテムの次のアイテムのポインタを取得
		m_pFree->m_pNext	= item->m_pNext;	// フリーアイテムの次のポインタに次のアイテムの次のポインタを設定

		item->m_pPrev			= m_pActive->m_pPrev;				// 現在アクティブなアイテムの前のアイテムのポインタを取得
		item->m_pNext			= m_pActive;						// 前のポインタの次のポインタに現在アクティブなポインタを設定
		item->m_pPrev->m_pNext	= item->m_pNext->m_pPrev = item;	// 前のポインタの見ている次のポインタに次のポインタの前のポインタを設定

		// カウンタを増やす
		m_Count++;

		// メモリ割り当て
		auto val = new((void*)item)T();

		// 初期化の必要があれば呼び出す
		if (_func != nullptr) { _func(item->m_Index, val); }

		return val;
	}

	// -------------------------------------------------------------------------------
	// @brief	アイテムを解放
	// 
	// @param[in]	pValue	解放するアイテムへのポインタ
	// -------------------------------------------------------------------------------
	void Free(T* _pValue)
	{
		if (_pValue == nullptr) 
		{ return; }

		std::lock_guard<std::mutex> guard(m_Mutex);

		// 解放するアイテムのポインタを取得
		auto item = reinterpret_cast<Item*>(_pValue);

		item->m_pPrev->m_pNext = item->m_pNext;	// アイテムの前のアイテムのポインタの次のアイテムのポインタに、アイテムの次のポインタを設定
		item->m_pNext->m_pPrev = item->m_pPrev;	// アイテムの次のアイテムのポインタの前のポインタに、アイテムの前のポインタを設定

		item->m_pPrev = nullptr;			// アイテムの前のポインタをnullptrにする
		item->m_pNext = m_pFree->m_pNext;	// アイテムの次のポインタは、FreeListの次のポインタに設定

		m_pFree->m_pNext = item;	// FreeListの次のポインタにアイテムのポインタを渡す
		m_Count--;					// カウントを減らす
	}

	// -------------------------------------------------------------------------------
	// @brief	総アイテム数を取得
	// 
	// @return	総アイテム数を返却
	// -------------------------------------------------------------------------------
	uint32_t GetSize() const 
	{ return m_Capacity; }

	// -------------------------------------------------------------------------------
	// @brief	使用中のアイテム数を取得
	// 
	// @return	使用中のアイテム数を返却
	// -------------------------------------------------------------------------------
	uint32_t GetUsedCount() const 
	{ return m_Count; }

	// -------------------------------------------------------------------------------
	// @brief	利用可能なアイテム数を取得
	// 
	// @return	利用可能なアイテム数を返却
	// -------------------------------------------------------------------------------
	uint32_t GetAvailableCount()const 
	{ return m_Capacity - m_Count; }


private:

	// -------------------------------------------------------------------------------
	// Item structure
	// -------------------------------------------------------------------------------
	struct Item
	{
		T			m_Value;	// 値
		uint32_t	m_Index;	// インデックス
		Item*		m_pNext;	// 次のアイテムへのポインタ
		Item*		m_pPrev;	// 前のアイテムへのポインタ

		Item()
			: m_Value()
			, m_Index(0)
			, m_pNext(nullptr)
			, m_pPrev(nullptr)
		{ /* DO_NOTHING */ }

		~Item()
		{ /* DO_NOTHING */ }
	};

	// -------------------------------------------------------------------------------
	// private variables
	// -------------------------------------------------------------------------------

	uint8_t*	m_pBuffer;		// バッファ
	Item*		m_pActive;		// アクティブアイテムの先頭
	Item*		m_pFree;		// フリーアイテムの先頭
	uint32_t	m_Capacity;		// 総アイテム数
	uint32_t	m_Count;		// 確保したアイテム数
	std::mutex	m_Mutex;		// ミューテックス

	// -------------------------------------------------------------------------------
	// private methods
	// -------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------
	// @brief	アイテムを取得
	// 
	// @param[in]	index	取得するアイテムのインデックス
	// @return	アイテムへのポインタを返却
	// -------------------------------------------------------------------------------
	Item* GetItem(uint32_t _index)
	{
		assert(0 <= _index && _index <= m_Capacity + 2);
		return reinterpret_cast<Item*>(m_pBuffer + sizeof(Item) * _index);
	}

	// -------------------------------------------------------------------------------
	// @brief	アイテムにメモリを割り当て
	// 
	// @param[in]	index	取得するアイテムのインデックス
	// @return	アイテムへのポインタを返却
	// -------------------------------------------------------------------------------
	Item* AssignItem(uint32_t _index)
	{
		assert(0 <= _index && _index <= m_Capacity + 2);
		auto buf = (m_pBuffer + sizeof(Item) * _index);
		return new(buf)Item;
	}

	Pool			(const Pool&) = delete;
	void operator = (const Pool&) = delete;

};

