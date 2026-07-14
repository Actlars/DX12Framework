#pragma once

namespace RHI
{
	// -------------------------------------------------------------------------------
	// BufferHeapType
	// 
	// 概要 : 
	//	バッファをどのヒープに置くかを表す。用途に応じて使い分ける
	//	- Upload	: CPUから書き込み可能、GPUから読み取り可能。CPU→GPUへ渡す入力データ向け
	//				（常時マップしておけるので、毎フレーム更新するデータにもおすすめ）
	//	- Default	: GPUのみが読み書きできる高速なVRAM。UAV(RWStructuredBUffer等)の
	//				書き込み先はここ。CPUからは直接読み取れない
	//  - Readback	: GPUが書き込み、CPUが読み取るための領域。UAVの実行結果を
	//				CPU側で確認したいときにDefaultヒープからReadbackヒープにコピーして使う
	// -------------------------------------------------------------------------------
	enum class BufferHeapType
	{
		Upload,
		Default,
		Readback,
	};

	// -------------------------------------------------------------------------------
	// BufferDesc
	// 
	// 概要 : 
	//	Buffer::Init()に渡す生成パラメータ
	// -------------------------------------------------------------------------------
	struct BufferDesc
	{
		size_t			SizeInBytes = 0;						// バッファ全体のサイズ
		BufferHeapType	HeapType	= BufferHeapType::Upload;	// 配置するヒープの種類
		bool			AllowUAV	= false;					// UAVとして使うかどうか（BufferHeapType::Defaultの時のみ意味を持つ）
	};

	// -------------------------------------------------------------------------------
	// Buffer class
	// 
	// 概要 : 
	//	ConstantBuffer専用ではない汎用バッファクラス
	//	StructuredBuffer / ByteAddressBuffer / RWStructuredBuffer などを
	//	RootDescriptor(SetComputeRootShaderResourceView等)としてバインドする用途を想定している
	// 
	//	ディスクリプタテーブル経由のSRV/UAVビューは作らない。RootDescriptorは
	//	「ビューを事前に発行する必要がなく、バインドのオーバーヘッドが小さい」という利点がある
	// -------------------------------------------------------------------------------
	class Buffer
	{
	public:

		Buffer() = default;
		~Buffer() { Term(); }

		// -------------------------------------------------------------------------------
		// @brief	Bufferを生成する
		// 
		// @param[in]	_pDevice	デバイス
		// @param[in]	_desc		生成パラメータ（サイズ・ヒープ種別・UAV有無）
		// @param[in]	_pInitData	初期データ（省略可）HeapType::Uploadの時のみ
		//							生成直後にCPUから即座にコピーされる
		//							Default/Readbackへの初期データ転送はしない
		//							（GPU側でCopyBufferRegion等を使って転送する）
		// -------------------------------------------------------------------------------
		bool Init(ID3D12Device* _pDevice, const BufferDesc _desc, const void* _pInitData = nullptr);

		// -------------------------------------------------------------------------------
		// @brief	終了
		// -------------------------------------------------------------------------------
		void Term();

		// -------------------------------------------------------------------------------
		// @brief	CPUから書き込む（Upload/Readbackヒープのみ対応、Defaultヒープは非対応）
		//			Init時点でマップされたままになっているので、マップしなおす必要はない
		// -------------------------------------------------------------------------------
		void Write(const void* _pData, size_t _size, size_t _offset = 0);

		// -------------------------------------------------------------------------------
		// @brief	CPUから読み取る（Readbackヒープのみ有効）
		//			GPU側の書き込み完了は呼び出し側が保証すること
		// -------------------------------------------------------------------------------
		void Read(void* _pOutData, size_t _size, size_t _offset = 0) const;

		// -------------------------------------------------------------------------------
		// @brief	RootDescriptor（SRV/UAV/CBV）としてバインドする際に使う
		//			GPU仮想アドレスを渡す
		// -------------------------------------------------------------------------------
		D3D12_GPU_VIRTUAL_ADDRESS GetAddress() const;

		// -------------------------------------------------------------------------------
		// @brief	リソース本体を返す（CopyBufferRegion等で直接ID3D12Resourceが必要な時に使う）
		// -------------------------------------------------------------------------------
		ID3D12Resource* GetResource()const { return m_pResource.Get(); }
		

	private:

		// -------------------------------------------------------------------------------
		// private variables
		// -------------------------------------------------------------------------------
		ComPtr<ID3D12Resource>	m_pResource;								// リソース本体
		void*					m_pMappedPtr	= nullptr;					// 常時マップしたポインタ
		size_t					m_Size			= 0;						// サイズ
		BufferHeapType			m_HeapType		= BufferHeapType::Default;	// ヒープタイプ

		Buffer			(const Buffer&) = delete;
		void operator = (const Buffer&) = delete;
	};

}
