#pragma once

// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "../Texture.h"
#include "../../Pool/DescriptorPool/DescriptorPool.h"

// -------------------------------------------------------------------------------
// TextureManager
// 
// 概要 : 
//		テクスチャのロード・キャッシュ・ライフタイム管理を行う
//		同一パスのテクスチャを2回ロードしないようにキャッシュする
//		各テクスチャはTextureクラスとして管理し
//		SRVはDescriptorPoolから割り当てる
// 
// 使い方:
//	TextureManager::InitDesc desc;
//	desc.pDevice		= pDevice;
//	desc.pQueue			= pQueue;
//	desc.pPool			= pPool;	DescriptorPoolを外から渡す
//	desc.MaxTextures	= 64;
//	
//	TextureManager mgr;
//	mgr.Init(desc);
// 
//	uint32_t slot;
//	mgr.Load("Assets/kerusi-.png",slot);
// 
//	const Texture* tex = mgr.GetBySlot(slot);
//	cmdList->SetGraphicsRootDescriptorTable(1, tex->GetHandleGPU());
// 
//	mgr.Term();
// -------------------------------------------------------------------------------
class TextureManager
{
public:

	// -------------------------------------------------------------------------------
	// 初期化情報
	// -------------------------------------------------------------------------------
	struct InitDesc
	{
		ID3D12Device*			pDevice;		// D3D12デバイス
		ID3D12CommandQueue*		pQueue;			// コピー用コマンドキュー
		DescriptorPool*			pPool;			// SRV割り当て元のDescriptorPool
		uint32_t				MaxTextures;	// 最大登録テクスチャ数
	};

	// -------------------------------------------------------------------------------
	//	コンストラクタ
	// -------------------------------------------------------------------------------
	TextureManager()	= default;

	// -------------------------------------------------------------------------------
	//	デストラクタ
	// -------------------------------------------------------------------------------
	~TextureManager()	= default;

	// -------------------------------------------------------------------------------
	// @brief	初期化
	//
	// @param[in]	_desc	初期化パラメータ
	// @retval	true	初期化成功
	// @retval	false	初期化失敗
	// -------------------------------------------------------------------------------
	bool Init(const InitDesc& _desc);

	// -------------------------------------------------------------------------------
	// @brief	終了処理。全テクスチャを解放する
	// -------------------------------------------------------------------------------
	void Term();

	// -------------------------------------------------------------------------------
	// @brief	テクスチャのロード
	// 
	//	同一パスがすでにロード済みの場合はキャッシュスロット番号を返し、
	//	GPUへのアップロードは行わない
	// 
	// @param[in]	_path    : ファイルパス
	// @param[out]	_outSlot : 割り当てられたスロット番号
	// @retval	treu	ロード成功（またはキャッシュヒット）
	// @retval	false	ロード失敗（ファイルなし・スロット上限）
	// -------------------------------------------------------------------------------
	bool Load(const std::wstring& _path, uint32_t& _outSlot);

	// -------------------------------------------------------------------------------
	// @brief	スロット番号でTextureを取得
	// 
	// @param[in]	_slot	Load()で返されたスロット番号
	// @return	Textureへのポインタ。スロットが無効ならnullptr
	// -------------------------------------------------------------------------------
	const Texture* GetBySlot(uint32_t _slot) const;

	// -------------------------------------------------------------------------------
	// @brief	ファイルパスでTextureを取得する（キャッシュから検索）
	// 
	// @param[in]	_path	Load()に渡したファイルパス
	// @return	Textureへのポインタ。未ロードならnullptr
	// -------------------------------------------------------------------------------
	const Texture* GetByPath(const std::wstring& _path) const;

	// -------------------------------------------------------------------------------
	// @brief	指定パスがすでにロード済みかを確認
	// 
	// @param[in]	_path	確認するファイルパス
	// @retval	true	ロード済み
	// @retval	false	未ロード
	// -------------------------------------------------------------------------------
	bool IsLoaded(const std::wstring& _path)const;

private:

	// -------------------------------------------------------------------------------
	// メンバ変数
	// -------------------------------------------------------------------------------
	
	// デバイス・キュー・プール（所有権なし、参照のみ）
	ID3D12Device*			m_pDevice		= nullptr;
	ID3D12CommandQueue*		m_pQueue		= nullptr;
	DescriptorPool*			m_pPool			= nullptr;
	uint32_t				m_MaxTextures	= 0;

	// スロット番号 → Texture（unique_ptrで所有権を明確化）
	std::vector<std::unique_ptr<Texture>>		m_Textures;

	// ファイルパス → スロット番号（キャッシュ・二重ロード防止）
	std::unordered_map<std::wstring, uint32_t>	m_PathToSlot;

	// 次に割り当てるスロット番号
	uint32_t m_NextSlot		= 0;

	// コピー禁止
	TextureManager	(const TextureManager&) = delete;
	void operator = (const TextureManager&) = delete;
};
