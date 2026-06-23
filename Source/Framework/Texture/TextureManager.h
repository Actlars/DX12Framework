#pragma once

using Microsoft::WRL::ComPtr;

// -------------------------------------------------------------------------------
// テクスチャリソース 1枚分の情報
// -------------------------------------------------------------------------------
struct TextureResource
{
	ComPtr<ID3D12Resource>		pResource;			// GPUリソース
	D3D12_CPU_DESCRIPTOR_HANDLE HandleCPU	= {};	// SRV CPUハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE HandleGPU	= {};	// SRV GPUハンドル
	uint32_t					Width		= 0;
	uint32_t					Height		= 0;
	DXGI_FORMAT					Format		= DXGI_FORMAT_UNKNOWN;
};

// -------------------------------------------------------------------------------
// TextureManager
// 
// 使い方:
// 1. Init() : デバイス・キュー・SRVヒープを渡す
// 2. Load() : パス指定ロード → HandleGPUをシェーダーにセット
// 3. Term() : まとめて解放
// 
// 対応フォーマット : DDS / PNG / JPG
// -------------------------------------------------------------------------------
class TextureManager
{
public:

	// -------------------------------------------------------------------------------
	// 初期化情報
	// -------------------------------------------------------------------------------
	struct InitDesc
	{
		ComPtr<ID3D12Device>			pDevice;		// D3D12デバイス
		ComPtr<ID3D12CommandQueue>		pQueue;			// コピー用コマンドキュー
		ComPtr<ID3D12DescriptorHeap>	pHeapSRV;		// SRV用ヒープ（SHADER_VISIBLE）
		uint32_t						BaseSlot;		// ヒープ内の開始スロット番号
		uint32_t						MaxTextures;	// 最大登録テクスチャ数
	};

	TextureManager()	= default;
	~TextureManager()	= default;

	// -------------------------------------------------------------------------------
	// 初期化 / 終了
	// -------------------------------------------------------------------------------
	bool Init(const InitDesc& _desc);
	void Term();

	// -------------------------------------------------------------------------------
	// テクスチャのロード
	// path    : ファイルパス
	// outSlot : ヒープ内のスロット番号を返す（シェーダーへのバインドに使う）
	// 戻り値  : 成功 = true / 失敗 = false
	// -------------------------------------------------------------------------------
	bool Load(const std::wstring& _path, uint32_t& _outSlot);

	// -------------------------------------------------------------------------------
	// テクスチャ情報の取得
	// キャッシュ済みのテクスチャをスロット番号 or パスで参照
	// -------------------------------------------------------------------------------
	const TextureResource* GetBySlot(uint32_t _slot)			const;
	const TextureResource* GetByPath(const std::wstring& _path) const;

	// -------------------------------------------------------------------------------
	// すでにロード済みかチェック（同一パスの二重ロード防止）
	// -------------------------------------------------------------------------------
	bool IsLoaded(const std::wstring& _path)const;

private:

	// -------------------------------------------------------------------------------
	// 内部ヘルパー
	// -------------------------------------------------------------------------------

	// DDSかどうか判定（拡張子で判別）
	static bool IsDDS(const std::wstring& _path);

	// DirectXTexのScratchImageからID3D12ResourceをGPUにアップロード
	bool UploadToGPU(
		const DirectX::ScratchImage&	_image,
		const DirectX::TexMetadata&		_meta,
		ComPtr<ID3D12Resource>&			_outResource
	);

	// SRVの登録
	void CreateSRV(
		const ComPtr<ID3D12Resource>&	_pResource,
		const DirectX::TexMetadata&		_meta,
		uint32_t						_slot,
		TextureResource&				_outTex
	);

	// -------------------------------------------------------------------------------
	// メンバ変数
	// -------------------------------------------------------------------------------
	ComPtr<ID3D12Device>			m_pDevice		= nullptr;
	ComPtr<ID3D12CommandQueue>		m_pQueue		= nullptr;
	ComPtr<ID3D12DescriptorHeap>	m_pHeapSRV		= nullptr;
	uint32_t						m_BaseSlot		= 0;
	uint32_t						m_MaxTextures	= 0;
	uint32_t						m_NextSlot		= 0;		// 次に割り当てるスロット
	uint32_t						m_IncrSize		= 0;		// SRVヒープのインクリメントサイズ

	// スロット番号 → テクスチャリソース
	std::vector<TextureResource>				m_Textures;

	// ファイルパス → スロット番号（キャッシュ用）
	std::unordered_map<std::wstring, uint32_t>	m_PathToSlot;
};
