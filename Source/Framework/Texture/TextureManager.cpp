// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "TextureManager.h"
#include <cassert>
#include <filesystem>

// -------------------------------------------------------------------------------
// 初期化
// -------------------------------------------------------------------------------
bool TextureManager::Init(const InitDesc& _desc)
{
	// assertで不正な入力を弾く
	{
		assert(_desc.pDevice);
		assert(_desc.pQueue);
		assert(_desc.pHeapSRV);
		assert(_desc.MaxTextures > 0);
	}

	// メンバ変数への代入
	{
		m_pDevice = _desc.pDevice;
		m_pQueue = _desc.pQueue;
		m_pHeapSRV = _desc.pHeapSRV;
		m_BaseSlot = _desc.BaseSlot;
		m_MaxTextures = _desc.MaxTextures;
		m_NextSlot = 0;					// まだ一枚もロードしていない状態
	}

	// インクリメントするサイズをGPUから取得（メーカーによって値が違うので、この関数で取得）
	m_IncrSize = m_pDevice->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Load()が使うメモリを事前に確保
	{
		// Load()の中で添え字アクセスするので、resizeを使用
		m_Textures.resize(m_MaxTextures);
		// unordered_mapで、Load()のたびに要素を追加していくので、事前にメモリを確保しておき、再確保の負荷をなくす
		m_PathToSlot.reserve(m_MaxTextures);
	}

	return true;
}

// -------------------------------------------------------------------------------
// 終了
// -------------------------------------------------------------------------------
void TextureManager::Term()
{
	// ComPtrなので自動解放されるがスロットを明示的にクリア
	for (auto& tex : m_Textures)
	{
		tex.pResource.Reset();
	}
	m_Textures.clear();
	m_PathToSlot.clear();
	m_NextSlot = 0;
}

// -------------------------------------------------------------------------------
// テクスチャのロード
// -------------------------------------------------------------------------------
bool TextureManager::Load(const std::wstring& _path, uint32_t& _outSlot)
{
	// キャッシュヒット : 同じパスがすでにロード済みなら即返す
	if (IsLoaded(_path))
	{
		_outSlot = m_PathToSlot[_path];
		return true;
	}

	// スロット上限チェック
	if (m_NextSlot >= m_MaxTextures)
	{
		assert(false && "TextureManager : スロット上限に達しました");
		return false;
	}

	// ファイル存在確認
	if (!std::filesystem::exists(_path))
	{
		return false;
	}

	// DirectXTexでロード
	DirectX::ScratchImage	scratchImage;	// テクスチャの仕様書。実施のピクセルデータは持たないが、情報を持つ。どんなテクスチャか
	DirectX::TexMetadata	meta;			// 実際のピクセルデータをCPUメモリ上に持つ

	HRESULT hr;

	// DDSの読み込み
	if (IsDDS(_path))
	{
		// DDS : BC圧縮・キューブマップ・ミップ付きをそのまま読む
		hr = DirectX::LoadFromDDSFile(
			_path.c_str(),				// std::wstringをwchar_tの生ポインタに変換して渡す
			DirectX::DDS_FLAGS_NONE,	// 特別な処理をしない。
			&meta,						// metaに書き込む。ロード後にmeta.widthなどで情報を参照できるように
			scratchImage
		);
	}
	else
	{
		hr = DirectX::LoadFromWICFile(
			_path.c_str(),
			DirectX::WIC_FLAGS_NONE,
			&meta,
			scratchImage
		);

		// WICはミップを持たないため自動生成
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
				scratchImage = std::move(mipped);	// mippedの中身をscratchImageに移動
				meta = scratchImage.GetMetadata();	// metaを更新。meta.mipLevelsの値が変わるため
			}
		}
	}

	if (FAILED(hr))
	{
		return false;
	}

	// GPUへアップロード
	const uint32_t		slot	= m_NextSlot;			// 現在の空きスロット番号を確定、m_NextSlotはまだ増やさない
	TextureResource&	tex		= m_Textures[slot];		// m_Textures配列の該当スロットへの参照

	if (!UploadToGPU(scratchImage, meta, tex.pResource))
	{
		return false;
	}

	// SRV生成
	CreateSRV(tex.pResource, meta, slot, tex);

	// 後でGetBySlot()した側がテクスチャはピクセルか、フォーマットを知れるようにする
	tex.Width	= static_cast<uint32_t>(meta.width);
	tex.Height	= static_cast<uint32_t>(meta.height);
	tex.Format	= meta.format;

	// キャッシュ登録
	m_PathToSlot[_path] = slot;	// 次回同じパスでLoad()されたときにキャッシュヒットするように
	_outSlot = slot;			// 呼び出し元にスロット番号を書き込み。このスロット番号でRender時にGPUハンドルを取得
	++m_NextSlot;				// 

	return true;

}

// -------------------------------------------------------------------------------
// スロット番号でテクスチャ取得
// -------------------------------------------------------------------------------
const TextureResource* TextureManager::GetBySlot(uint32_t _slot) const
{
	if (_slot >= m_NextSlot) { return nullptr; }
	return &m_Textures[_slot];
}

// -------------------------------------------------------------------------------
// パスでテクスチャ取得
// -------------------------------------------------------------------------------
const TextureResource* TextureManager::GetByPath(const std::wstring& _path) const
{
	auto it = m_PathToSlot.find(_path);
	if (it == m_PathToSlot.end()) { return nullptr; }
	return &m_Textures[it->second];
}

// -------------------------------------------------------------------------------
// ロード済み確認
// -------------------------------------------------------------------------------
bool TextureManager::IsLoaded(const std::wstring& _path) const
{
	return m_PathToSlot.count(_path) > 0;
}

// -------------------------------------------------------------------------------
// private ヘルパー
// -------------------------------------------------------------------------------

// -------------------------------------------------------------------------------
// 拡張子でDDSか判定
// -------------------------------------------------------------------------------
bool TextureManager::IsDDS(const std::wstring& _path)
{
	const auto ext = std::filesystem::path(_path).extension().wstring();
	// 大文字小文字どちらでも対応
	std::wstring extLower = ext;
	for (auto& c : extLower) { c = static_cast<wchar_t>(towlower(c)); }
	return extLower == L".dds";
}

// -------------------------------------------------------------------------------
// GPUへのアップロード
// UPLOADヒープ（中間バッファ） → DEFAULTヒープ（GPUネイティブ）のコピーフロー
// ResourceUploadBatchを使わず主導でやることで
// フレームワーク外部ライブラリへの依存を小さくしている
// 
// 1. CPUメモリ上のピクセルデータをD3D12_SUBRESOURCE_DATAに整理
// 2. DEFAULTヒープにGPUリソースを作成
// 3. UPLOADヒープに中間バッファを作成（CPUが書ける一時領域）
// 4. ロード専用CommandListを作成
// 5. UpdateSubresourcesでCPUデータをUPLOADバッファに書き込み、コピー命令をCommandListに積む
// 6. バリアを積む（COPY_DEST → PIXEL_SHADER_RESOURCE）
// 7. CommandListをClose → ExecuteCommandListsでGPUに投入
// 8. フェンスで完了を待つ
// 9. 関数を抜けるとpUploadBufferが自動解放、_outResourceにGPU上のテクスチャが入った状態で呼び出し元に返る
// -------------------------------------------------------------------------------
bool TextureManager::UploadToGPU(
	const DirectX::ScratchImage&	_image,			// CPU上のピクセルデータ
	const DirectX::TexMetadata&		_meta,			// 仕様書
	ComPtr<ID3D12Resource>&			_outResource)	// 転送先のGPUリソースを返す出力引数
{
	// サブリソースのレイアウト計算
	// GPUへのコピー命令はサブリソース単位で指定するため、総数が必要
	const uint32_t subresourceCount =
		static_cast<uint32_t>(_meta.mipLevels * _meta.arraySize);
	
	// D3D12_SUBRESOURCE_DATAはサブリソース1つ分のどこにデータがあるかを表す構造体
	std::vector<D3D12_SUBRESOURCE_DATA> subresources(subresourceCount);
	for (uint32_t i = 0; i < subresourceCount; ++i)
	{
		// サブリソースi番目に対応する画像を取得
		// インデックスiからミップと配列インデックスを逆算
		const auto* img = _image.GetImage(
			i % _meta.mipLevels,
			i / _meta.mipLevels,
			0);
		subresources[i].pData = img->pixels;									// CPUメモリ上のピクセルデータの先頭ポインタ
		subresources[i].RowPitch = static_cast<LONG_PTR>(img->rowPitch);		// 横1行のバイト数。1024*1024のRGBA8なら1024*4バイト = 4096
		subresources[i].SlicePitch = static_cast<LONG_PTR>(img->slicePitch);	// 1枚全体のバイト数。2DテクスチャならrowPitch * height
	}

	// DEFAULTヒープにGPUリソースを作成
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
	auto hr = m_pDevice->CreateCommittedResource(
		&heapDefault,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,				// このリソースはコピー先として使うという初期状態の宣言
		nullptr,									// クリアカラー
		IID_PPV_ARGS(_outResource.GetAddressOf()));	// COMオブジェクト生成時に使うマクロ。インターフェースのGUIDとポインタのアドレスを同時に渡す
	if (FAILED(hr)) 
	{ return false; }

	// UPLOADヒープに中間バッファを作成
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
	{ return false; }

	// コマンドアロケータ＆リスト（ロード専用 / 使い捨て）
	// ロード専用のCommandListを作る理由は描画用のCommandListと分離して描画中のCommandListにコピー命令を混ぜないため
	ComPtr<ID3D12CommandAllocator>		pCmdAlloc;	// コマンドリストが記録するコマンドの実際のメモリ領域
	ComPtr<ID3D12GraphicsCommandList>	pCmdList;
	ComPtr<ID3D12Fence>					pFence;

	hr = m_pDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(pCmdAlloc.GetAddressOf()));
	if (FAILED(hr)) 
	{ return false; }

	hr = m_pDevice->CreateCommandList(
		0,										// 0はNodeMask。マルチGPU構成でも使うもの
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		pCmdAlloc.Get(),
		nullptr,								// PipelineState。コピーはシェーダーを使わないのでnullptr
		IID_PPV_ARGS(pCmdList.GetAddressOf()));
	if (FAILED(hr)) 
	{ return false; }

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
	barrier.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;		// リソースの用途を切り替えるバリア
	barrier.Transition.pResource	= _outResource.Get();
	barrier.Transition.StateBefore	= D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter	= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;		// 全ミップ・全配列スロットを一括で遷移
	pCmdList->ResourceBarrier(1, &barrier);

	// コマンドリストをクローズして実行
	hr = pCmdList->Close();	// 記録を終了
	if (FAILED(hr)) 
	{ return false; }

	// ExecuteCommandListsはCommandQueueにコマンドリストを投入
	// この関数はCPU側ではすぐ帰る。GPUが実際に実行するのはこれ以降の非同期のタイミング
	ID3D12CommandList* ppCmdLists[] = { pCmdList.Get() };
	m_pQueue->ExecuteCommandLists(1, ppCmdLists);			// 配列で渡しているのは複数のCommandListをまとめて投入できるAPIだから

	// GPU完了をフェンスで同期待ち
	hr = m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(pFence.GetAddressOf()));
	if (FAILED(hr)) 
	{ return false; }

	// WindowsのイベントオブジェクトをOSに作らせている
	// WaitForSingleObjectでこのイベントが通知されるまでCPUスレッドをスリープ
	HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!hEvent) { return false; }

	// CommandQueueの処理がここまで来たらフェンスの値を1にセットせよという命令をGPUに投入
	m_pQueue->Signal(pFence.Get(), 1);
	pFence->SetEventOnCompletion(1, hEvent);	// フェンスの値が1になったらhEventを通知
	WaitForSingleObject(hEvent, INFINITE);		// CPUスレッドをスリープさせる
	CloseHandle(hEvent);						// CPUが起きる

	// pUploadBufferはGPU完了後に解放される（ComPtrがスコープアウト時に自動）
	// GPUがコピーを終える前にUPLOADバッファが消えると壊れたデータが転送される
	// そのため、フェンスで待つことでGPUがコピーを終えてから関数を抜けるようにする

	return true;
}

// -------------------------------------------------------------------------------
// SRVの生成
// -------------------------------------------------------------------------------
void TextureManager::CreateSRV(
	const ComPtr<ID3D12Resource>&	_pResource,
	const DirectX::TexMetadata&		_meta,
	uint32_t						_slot,
	TextureResource&				_outTex)
{
	const uint32_t heapSlot = m_BaseSlot + _slot;

	// ハンドル計算
	auto handleCPU = m_pHeapSRV->GetCPUDescriptorHandleForHeapStart();
	auto handleGPU = m_pHeapSRV->GetGPUDescriptorHandleForHeapStart();
	handleCPU.ptr += static_cast<SIZE_T>(m_IncrSize) * heapSlot;
	handleGPU.ptr += static_cast<UINT64>(m_IncrSize) * heapSlot;

	// SRVDesc
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format					= _meta.format;

	if (_meta.IsCubemap())
	{
		srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels			= static_cast<UINT>(_meta.mipLevels);
		srvDesc.TextureCube.MostDetailedMip		= 0;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	}
	else if (_meta.arraySize > 1)
	{
		srvDesc.ViewDimension						= D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MipLevels			= static_cast<UINT>(_meta.mipLevels);
		srvDesc.Texture2DArray.MostDetailedMip		= 0;
		srvDesc.Texture2DArray.ArraySize			= static_cast<UINT>(_meta.arraySize);
		srvDesc.Texture2DArray.FirstArraySlice		= 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp	= 0.0f;
	}
	else
	{
		srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels				= static_cast<UINT>(_meta.mipLevels);
		srvDesc.Texture2D.MostDetailedMip		= 0;
		srvDesc.Texture2D.ResourceMinLODClamp	= 0.0f;
	}

	m_pDevice->CreateShaderResourceView(_pResource.Get(), &srvDesc, handleCPU);

	_outTex.HandleCPU = handleCPU;
	_outTex.HandleGPU = handleGPU;
}
