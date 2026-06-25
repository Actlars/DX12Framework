// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Texture.h"

// -------------------------------------------------------------------------------
//	初期化
// -------------------------------------------------------------------------------
bool Texture::Init(
	ID3D12Device*		_pDevice,
	ID3D12CommandQueue* _pQueue,
	DescriptorPool*		_pPool,
	const std::wstring& _path)
{
	// 引数チェック
	assert(_pDevice != nullptr);
	assert(_pQueue != nullptr);
	assert(_pPool != nullptr);

	// ファイル存在確認
	// 存在しないパスを渡されたら早期リターンで無駄な処理を避ける
	if (!std::filesystem::exists(_path))
	{
		return false;
	}

	// メンバ変数に保持（所有権はなく参照のみ）
	m_pDevice	= _pDevice;
	m_pQueue	= _pQueue;
	m_pPool		= _pPool;

	// DirectXTexでファイルをCPUメモリにロード
	DirectX::ScratchImage	scratchImage;
	DirectX::TexMetadata	meta;
	HRESULT hr;

	if (IsDDS(_path))
	{
		// DDS : BC圧縮済み・ミップ付き・キューブマップをそのまま読む
		// GPUが直接扱えるデータなので加工不要
		hr = DirectX::LoadFromDDSFile(
			_path.c_str(),				// std::wstringをwchar_tの生ポインタに変換して渡す
			DirectX::DDS_FLAGS_NONE,	// 特別な処理をしない。
			&meta,						// metaに書き込む。ロード後にmeta.widthなどで情報を参照できるように
			scratchImage);
	}
	else
	{
		// PNG/JPG/BMP/TGA/HDR : WICでデコードしてRGBA生データにする
		hr = DirectX::LoadFromWICFile(
			_path.c_str(),
			DirectX::WIC_FLAGS_NONE,
			&meta,
			scratchImage
		);

		// WICフォーマットはミップを持たないため自動生成する
		// ミップがないと遠距離オブジェクトがちらつく（エイリアシング）
		if (SUCCEEDED(hr) && meta.mipLevels <= 1)
		{
			DirectX::ScratchImage mipped;
			hr = DirectX::GenerateMipMaps(
				*scratchImage.GetImage(0, 0, 0),	// （ミップレベル, 配列インデックス, スライス）
				DirectX::TEX_FILTER_DEFAULT,		// バイリニアフィルターでミップを生成
				0,									// 0 = フルミップチェーン自動計算
				mipped
			);

			if (SUCCEEDED(hr))
			{
				scratchImage = std::move(mipped);	// mippedの中身をscratchImageに移動。std::moveで所有権を移動（コピーコストゼロ）
				meta = scratchImage.GetMetadata();	// ミップ生成後にmeta.mipLevelsが変わるので更新
			}
		}
	}

	if (FAILED(hr))
	{
		return false;
	}

	// GPUへのアップロード
	if (!UploadToGPU(scratchImage, meta, m_pResource)) 
	{ return false; }

	// SRVをDescriptorPoolに登録
	CreateSRV(meta);

	// メタ情報を保持（呼び出し側が参照できるように）
	m_Width		= static_cast<uint32_t>(meta.width);
	m_Height	= static_cast<uint32_t>(meta.height);
	m_Format	= meta.format;

	return true;
}

// -------------------------------------------------------------------------------
// 終了処理
// -------------------------------------------------------------------------------
void Texture::Term()
{
	// DescriptorPool にSRVハンドルを返却
	// プールから借りたハンドルは必ず返す
	// プールがnullptrの場合（Init未完了）は何もしない
	if (m_pPool != nullptr && m_pHandle != nullptr)
	{
		m_pPool->FreeHandle(m_pHandle);
		m_pHandle = nullptr;
	}

	// ─── GPUリソースの解放 ───
	// ComPtrなので参照カウントが0になったとき自動解放される
	m_pResource.Reset();

	// ─── ポインタのクリア ───
	m_pDevice	= nullptr;
	m_pQueue	= nullptr;
	m_pPool		= nullptr;

	m_Width		= 0;
	m_Height	= 0;
	m_Format	= DXGI_FORMAT_UNKNOWN;
}

// -------------------------------------------------------------------------------
// GPUハンドルの取得
// -------------------------------------------------------------------------------
D3D12_GPU_DESCRIPTOR_HANDLE Texture::GetHandleGPU() const
{
	// ハンドルが未確保の場合は無効な値（ptr = 0）を返す
	if (m_pHandle == nullptr) 
	{ return D3D12_GPU_DESCRIPTOR_HANDLE{ 0 }; }

	return m_pHandle->HandleGPU;
}

// -------------------------------------------------------------------------------
// CPUハンドルの取得
// -------------------------------------------------------------------------------
D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetHandleCPU() const
{
	if (m_pHandle == nullptr) 
	{ return D3D12_CPU_DESCRIPTOR_HANDLE{ 0 }; }

	return m_pHandle->HandleCPU;
}

// -------------------------------------------------------------------------------
// 横幅の取得
// -------------------------------------------------------------------------------
uint32_t Texture::GetWidth() const 
{ return m_Width; }

// -------------------------------------------------------------------------------
// 縦幅の取得
// -------------------------------------------------------------------------------
uint32_t Texture::GetHeight() const 
{ return m_Height; }

// -------------------------------------------------------------------------------
// フォーマットの取得
// -------------------------------------------------------------------------------
DXGI_FORMAT Texture::GetFormat() const 
{ return m_Format; }

// -------------------------------------------------------------------------------
// GPUリソースの取得
// -------------------------------------------------------------------------------
ID3D12Resource* Texture::GetResource() const 
{ return m_pResource.Get(); }

// -------------------------------------------------------------------------------
// private
// -------------------------------------------------------------------------------

// -------------------------------------------------------------------------------
// DDSかどうかを拡張子で判定
// -------------------------------------------------------------------------------
bool Texture::IsDDS(const std::wstring& _path)
{
	// std::filesystem::path で拡張子を取りだす
	auto ext = std::filesystem::path(_path).extension().wstring();

	// 大文字・小文字どちらでも対応（.DDS / .dds）
	for (auto& c : ext) 
	{ c = static_cast<wchar_t>(towlower(c)); }

	return ext == L".dds";
}

// -------------------------------------------------------------------------------
// GPUへのアップロード
// -------------------------------------------------------------------------------
bool Texture::UploadToGPU(
	const DirectX::ScratchImage&	_image,
	const DirectX::TexMetadata&		_meta,
	ComPtr<ID3D12Resource>&			_outResource)
{
	// サブリソース総数の計算
	// サブリソース = ミップ段数 * 配列サイズ
	const uint32_t subresourceCount =
		static_cast<uint32_t>(_meta.mipLevels * _meta.arraySize);

	// D3D12_SUBRESOURCE_DATAの配列を構築
	// 各サブリソースのCPUメモリ上のアドレスとピッチ情報をまとめる
	std::vector<D3D12_SUBRESOURCE_DATA> subresources(subresourceCount);
	for (uint32_t i = 0; i < subresourceCount; ++i)
	{
		// GetImage(mipLevel, arrayIndex, slice)
		// i番目のサブリソースをミップと配列インデックスに分解して取得
		const auto* img = _image.GetImage(
			i % _meta.mipLevels,	// ミップレベル
			i / _meta.mipLevels,	// 配列インデックス
			0);						// スライス（3Dテクスチャ以外は0固定）

		subresources[i].pData = img->pixels;
		subresources[i].RowPitch = static_cast<LONG_PTR>(img->rowPitch);
		subresources[i].SlicePitch = static_cast<LONG_PTR>(img->slicePitch);
	}

	// デフォルトヒープにGPUリソースを作成（コピー先）
	D3D12_HEAP_PROPERTIES heapDefault = {};		// GPUリソースをどの種類のメモリに置くかを指定する構造体
	heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;	// D3D12_HEAP_TYPE_DEFAULTはGPU専用メモリ（VRAM）のこと。CPUからは直接書き込めないが、GPUからの読み取りが高速

	D3D12_RESOURCE_DESC resDesc = {};	// D3D12_RESOURCE_DESCはGPUリソースの形状を定義する構造体
	resDesc.Dimension			= (_meta.dimension == DirectX::TEX_DIMENSION_TEXTURE3D)		// Dimensionはリソースが何次元かを表す
									? D3D12_RESOURCE_DIMENSION_TEXTURE3D
									: D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Width				= static_cast<UINT64>(_meta.width);
	resDesc.Height				= static_cast<UINT>(_meta.height);
	resDesc.DepthOrArraySize	= (_meta.dimension == DirectX::TEX_DIMENSION_TEXTURE3D)		// 3Dテクスチャなら奥行、2Dテクスチャなら配列枚数を表す
									? static_cast<UINT16>(_meta.depth)
									: static_cast<UINT16>(_meta.arraySize);
	resDesc.MipLevels			= static_cast<UINT16>(_meta.mipLevels);						// ミップマップの段数。GPUはこの数だけメモリを確保
	resDesc.Format				= _meta.format;
	resDesc.SampleDesc.Count	= 1;														// マルチサンプリング（MSAA）の設定
	resDesc.SampleDesc.Quality	= 0;														// テクスチャには不要なので、Count = 1, Quality = 0が通常設定
	resDesc.Layout				= D3D12_TEXTURE_LAYOUT_UNKNOWN;								// LayoutはGPUメモリ上のピクセル配置方法。UNKNOWNを指定するとGPUドライバーが最適なレイアウトを自動選択
	resDesc.Flags				= D3D12_RESOURCE_FLAG_NONE;									// RenderTargetやUAVとして使う場合に指定。読み取り専用のテクスチャなのでNONE

	// ヒープとリソースを同時に作成
	// 初期状態をCOPY_DESTにする（コピーを受け取る前提）
	auto hr = m_pDevice->CreateCommittedResource(
		&heapDefault,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,				// このリソースはコピー先として使うという初期状態の宣言
		nullptr,									// クリアカラー
		IID_PPV_ARGS(_outResource.GetAddressOf()));	// COMオブジェクト生成時に使うマクロ。インターフェースのGUIDとポインタのアドレスを同時に渡す
	if (FAILED(hr))
	{
		return false;
	}

	// UPLOADヒープに中間バッファを作成
	// CPU が書き込み、GPU がここから DEFAULT ヒープにコピーする
	
    // バッファサイズはGPUのアライメント要件があるためAPIに計算させる
	// 作成したGPUリソースに全サブリソースをコピーするために必要な中間バッファのサイズをバイト単位で返す
	const UINT64 uploadSize = GetRequiredIntermediateSize(
		_outResource.Get(), 0, subresourceCount);

	// UPLOADヒープはCPUとGPUの両方からアクセスできる共有メモリ領域
	// CPUがここにデータを書き込み、GPUがここから読み取ってDEFAULTヒープにコピーする
	D3D12_HEAP_PROPERTIES heapUpload = {};
	heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;

	// 中間バッファはテクスチャではなくただのバイト列（Buffer）として作る
	D3D12_RESOURCE_DESC uploadDesc = {};
	uploadDesc.Dimension		= D3D12_RESOURCE_DIMENSION_BUFFER;	// メモリを1次元配列として扱う
	uploadDesc.Width			= uploadSize;
	uploadDesc.Height			= 1;								// Height,DepthOrArraySize,MipLevelsはBufferの場合は1固定
	uploadDesc.DepthOrArraySize = 1;
	uploadDesc.MipLevels		= 1;
	uploadDesc.Format			= DXGI_FORMAT_UNKNOWN;				// Bufferはピクセルフォーマットを持たないのでUNKNOWN
	uploadDesc.SampleDesc.Count = 1;
	uploadDesc.Layout			= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;	// Bufferはメモリを先頭から順番に並べる（行優先）という意味
	uploadDesc.Flags			= D3D12_RESOURCE_FLAG_NONE;

	ComPtr<ID3D12Resource> pUploadBuffer;
	hr = m_pDevice->CreateCommittedResource(
		&heapUpload,
		D3D12_HEAP_FLAG_NONE,
		&uploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(pUploadBuffer.GetAddressOf()));
	if (FAILED(hr))
	{
		return false;
	}

	// コマンドアロケータ＆リスト（ロード専用 / 使い捨て）
	// ロード専用のCommandListを作る理由は描画用のCommandListと分離して描画中のCommandListにコピー命令を混ぜないため
	ComPtr<ID3D12CommandAllocator>		pCmdAlloc;	// コマンドリストが記録するコマンドの実際のメモリ領域
	ComPtr<ID3D12GraphicsCommandList>	pCmdList;
	ComPtr<ID3D12Fence>					pFence;

	hr = m_pDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(pCmdAlloc.GetAddressOf()));
	if (FAILED(hr))
	{
		return false;
	}

	hr = m_pDevice->CreateCommandList(
		0,										// 0はNodeMask。マルチGPU構成でも使うもの
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		pCmdAlloc.Get(),
		nullptr,								// PipelineState。コピーはシェーダーを使わないのでnullptr
		IID_PPV_ARGS(pCmdList.GetAddressOf()));
	if (FAILED(hr))
	{
		return false;
	}

	// サブリソースをコピー（UpdateSubresources は d3dx12.hのヘルパー）
	// 1. UPLOADバッファにCPうからピクセルデータをmemcpyする
	// 2. CommandListにCopyTextureRegion命令を積む（UPLOADバッファ → DEFAULTリソースへのコピー命令）
	UpdateSubresources(
		pCmdList.Get(),
		_outResource.Get(),
		pUploadBuffer.Get(),
		0, 0,					// UPLOADバッファのオフセットは先頭から使うので0。コピー開始サブリソース番号は0番目からコピー
		subresourceCount,
		subresources.data());

	// リソースバリア : COPY_DEST → PIXEL_SHADER_RESOURCE
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;		// リソースの用途を切り替えるバリア
	barrier.Transition.pResource = _outResource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;		// 全ミップ・全配列スロットを一括で遷移
	pCmdList->ResourceBarrier(1, &barrier);

	// コマンドリストをクローズして実行
	hr = pCmdList->Close();	// 記録を終了
	if (FAILED(hr))
	{
		return false;
	}

	// ExecuteCommandListsはCommandQueueにコマンドリストを投入
	// この関数はCPU側ではすぐ帰る。GPUが実際に実行するのはこれ以降の非同期のタイミング
	ID3D12CommandList* ppCmdLists[] = { pCmdList.Get() };
	m_pQueue->ExecuteCommandLists(1, ppCmdLists);			// 配列で渡しているのは複数のCommandListをまとめて投入できるAPIだから

	// GPU完了をフェンスで同期待ち
	hr = m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(pFence.GetAddressOf()));
	if (FAILED(hr))
	{
		return false;
	}

	// WindowsのイベントオブジェクトをOSに作らせている
	// WaitForSingleObjectでこのイベントが通知されるまでCPUスレッドをスリープ
	HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!hEvent) { return false; }

	// CommandQueueの処理がここまで来たらフェンスの値を1にセットせよという命令をGPUに投入
	m_pQueue->Signal(pFence.Get(), 1);
	pFence->SetEventOnCompletion(1, hEvent);	// フェンスの値が1になったらhEventを通知
	WaitForSingleObject(hEvent, INFINITE);		// CPUスレッドをスリープさせてGPU完了を待つ
	CloseHandle(hEvent);						// CPUが起きる

	// pUploadBufferはGPU完了後に解放される（ComPtrがスコープアウト時に自動）
	// GPUがコピーを終える前にUPLOADバッファが消えると壊れたデータが転送される
	// そのため、フェンスで待つことでGPUがコピーを終えてから関数を抜けるようにする

	return true;
}

// -------------------------------------------------------------------------------
// SRVの作成とDescriptorPoolへの登録
// -------------------------------------------------------------------------------
void Texture::CreateSRV(const DirectX::TexMetadata& _meta)
{
	// ─── DescriptorPool からSRVハンドルを1つ借りる ───
	m_pHandle = m_pPool->AllocHandle();
	assert(m_pHandle != nullptr && "DescriptorPool のスロットが不足しています");

	// ─── SRV Desc の構築 ───
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = _meta.format;

	// テクスチャの種類（キューブマップ / 配列 / 通常2D）を自動判別
	if (_meta.IsCubemap())
	{
		// キューブマップ: スカイボックス等に使う。6面を1リソースで管理する
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = static_cast<UINT>(_meta.mipLevels);
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	}
	else if (_meta.arraySize > 1)
	{
		// 配列テクスチャ: 複数枚を1リソースにまとめたもの
		// AnimToTextureのBAT（BoneAnimationTexture）等に使う
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MipLevels = static_cast<UINT>(_meta.mipLevels);
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.ArraySize = static_cast<UINT>(_meta.arraySize);
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
	}
	else
	{
		// 通常のTexture2D
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = static_cast<UINT>(_meta.mipLevels);
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	}

	// ─── SRV をDescriptorPool が持つヒープのスロットに書き込む ───
	m_pDevice->CreateShaderResourceView(
		m_pResource.Get(),
		&srvDesc,
		m_pHandle->HandleCPU);
}