#pragma once

namespace EditorUI
{
	// 変数テンプレート
	template<class T> constexpr T fnv_prime;
	template<class T> constexpr T fnv_offset_basis;

	// template<>はこれは一般の型ではなく、特定の型に対する専用の実装/値という意味
	// T = uint32_tのときはこの値を使うというような個別の値を登録
	template<> constexpr std::uint32_t fnv_prime<std::uint32_t>			= 16777619u;
	template<> constexpr std::uint64_t fnv_prime<std::uint64_t>			= 1099511628211ull;
	template<> constexpr std::uint32_t fnv_offset_basis<std::uint32_t>	= 2166136261u;
	template<> constexpr std::uint64_t fnv_offset_basis<std::uint64_t>	= 14695981039346656037ull;

	// -------------------------------------------------------------------------------
	// fnv1a_hash（文字列版）
	// 
	// 概要 : 
	//	文字列に対するFNV-1aハッシュ関数
	//	同じ文字列・同じ_seedを渡せば、、いつよんでも必ず同じ数値が返る
	// -------------------------------------------------------------------------------
	template<class T>
	constexpr T fnv1a_hash(std::string_view _str, T _seed = fnv_offset_basis<T>)
	{
		T hash = _seed;
		for (auto c : _str)
		{
			// １文字ずつXOR→乗算を繰り返すことで、入力のわずかな違いが
			// 出力全体に大きく広がるようにする
			hash ^= static_cast<unsigned char>(c);
			hash *= fnv_prime<T>;
		}
		return hash;
	}

	// -------------------------------------------------------------------------------
	// fnv1a_hash（生バイト列版）
	// 
	// 概要 : 
	//	文字列以外をハッシュ化したい場合に使う
	// -------------------------------------------------------------------------------
	template<class T>
	constexpr T fnv1a_hash(const unsigned char* _data, std::size_t _size, T _seed = fnv_offset_basis<T>) noexcept
	{
		T hash = _seed;
		for (std::size_t i = 0; i < _size; ++i)
		{
			hash ^= _data[i];
			hash *= fnv_prime<T>;
		}
		return hash;
	}

	// IDのビット幅はここを変えるだけで切り替え可能
	using Id = std::uint32_t;

	// 文字列→Idへの変換の入り口
	inline Id HashString(std::string_view _str, Id _seed = fnv_offset_basis<Id>)noexcept
	{
		return fnv1a_hash<Id>(_str, _seed);
	}

	// -------------------------------------------------------------------------------
	// HashValue
	// 
	// 概要 : 
	//	trivially copyableな値全般を同じ経路でハッシュ化するための共通関数
	// -------------------------------------------------------------------------------
	template<typename T>
	inline Id HashValue(const T& _value, Id _seed = fnv_offset_basis<Id>) noexcept
	{
		// std::is_trivially_copyableはTがメモリコピーだけで安全に複製できる型かどうかをコンパイル時に判定する
		//static_assert(std::is_trivially_copyable_v<T>, "HashValue : trivially copyable な型のみ対応");
		// reinterpret_cast<const unsigned char*>(&_value)
		// &_valueでT型オブジェクトの先頭アドレスを取り、それを１バイトずつの配列として無理やり読み替えるキャスト
		return fnv1a_hash<Id>(reinterpret_cast<const unsigned char*>(&_value), sizeof(T), _seed);
	}

	// ループのindex等をスコープとして積むためのハッシュ
	inline Id HashInt(int _value, Id _seed = fnv_offset_basis<Id>) noexcept
	{
		return HashValue(_value, _seed);
	}

	// 配列要素へのポインタ等でユニーク化したい場合のハッシュ
	inline Id HashPtr(const void* _ptr, Id _seed = fnv_offset_basis<Id>) noexcept
	{
		return HashValue(_ptr, _seed);
	}

	// -------------------------------------------------------------------------------
	// IdStack class
	// 
	// 概要 : 
	//	Begin / Endでネストするスコープに応じたIdを発行するためのクラス
	//	スタックの先頭(m_Stack.back())が現在のスコープのシード値であり、PushXxxで新しいスコープに入り、Popで抜ける
	//	エディタのUI部品自身（ウィンドウ、ボタン、スライダー、テキストボックス等）を指す
	// -------------------------------------------------------------------------------
	class IdStack
	{
	public:

		IdStack()	{ m_Stack.push_back(fnv_offset_basis<Id>); }
		~IdStack()	{ /* DO_NOTHING */ }

		// 現在のスコープを基準にIdを計算するだけで、スタックそのものは変化させない
		Id GetId(std::string_view _label)	const { return HashString(_label, m_Stack.back()); }
		Id GetId(int _intId)				const { return HashInt(_intId, m_Stack.back()); }
		Id GetId(const void* _ptr)			const { return HashPtr(_ptr, m_Stack.back()); }	

		// GetIdで計算したIdをスタックに積み、以降のGetIdの基準（seed）を切り替える
		void PushString(std::string_view _label)	{ m_Stack.push_back(GetId(_label)); }
		void PushInt(int _indId)					{ m_Stack.push_back(GetId(_indId)); }
		void PushPtr(const void* _ptr)				{ m_Stack.push_back(GetId(_ptr)); }

		void Pop()
		{
			assert(m_Stack.size() > 1 && "IdStack : 対応するPushがないPop");
			m_Stack.pop_back();
		}

		void Clear()
		{
			m_Stack.clear();
			m_Stack.push_back(fnv_offset_basis<Id>);
		}

	private:

		std::vector<Id> m_Stack;
	};


}
