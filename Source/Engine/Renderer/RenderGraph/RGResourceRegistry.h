#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "RGTypes.h"
#include <Engine/RHI/Resource/Texture/TransientResourcePool/TransientResourcePool.h>
#include <Engine/RHI/Resource/DescriptorHeap/DescriptorPool/DescriptorPool.h>

namespace RG
{
	// -------------------------------------------------------------------------------
	// ResourceRegistry
	// 
	// 概要 : 
	//	RenderGraphが1フレームの中で扱うリソースの一覧を管理する
	//	現状はImport（外部から持ち込むリソース）のみ対応
	//	後でグラフ内で生成し、フレーム末に破棄する一時リソースも作る
	// -------------------------------------------------------------------------------
	class ResourceRegistry
	{
	public:

		// -------------------------------------------------------------------------------
		// @brief	外部生成済みのリソースをグラフに取り込む
		//			BeginFrameで確定するバックバッファ/DepthTarget等に使う
		// -------------------------------------------------------------------------------
		Handle Import(const std::string& _name, ID3D12Resource* _pResource);

		// -------------------------------------------------------------------------------
		// @brief	一時リソースの生成を要求する。実体はプールから確保される。
		// -------------------------------------------------------------------------------
		Handle CreateTransient(
			const std::string&				_name,
			const TransientResourceDesc&	_desc,
			RHI::TransientResourcePool*		_pResourcePool);

		// -------------------------------------------------------------------------------
		// @brief	一時リソースの生成を要求する（RTV + SRVの両方を発行）
		//			他のパスから読み取られる中間バッファに使う
		// -------------------------------------------------------------------------------
		Handle CreateTransient(
			ID3D12Device*					_pDevice,
			const std::string&				_name,
			const TransientResourceDesc&	_desc,
			RHI::TransientResourcePool*		_pResourcePool,
			RHI::DescriptorPool*			_pRTVPool,
			RHI::DescriptorPool*			_pSRVPool);

		// -------------------------------------------------------------------------------
		// @brief	一時リソースの生成を要求する（RTVのみ発行、SRVは作らない）
		//			書き込み専用で、後続パスから読まれることがない場合に使う
		//			SRV用ディスクリプタを消費しない分、メモリ使用量が少ない
		// -------------------------------------------------------------------------------
		Handle CreateTransient(
			ID3D12Device*					_pDevice,
			const std::string&				_name,
			const TransientResourceDesc&	_desc,
			RHI::TransientResourcePool*		_pResourcePool,
			RHI::DescriptorPool*			_pRTVPool);

		// -------------------------------------------------------------------------------
		// @brief	グラフからハンドルに対応したリソースを取得
		// -------------------------------------------------------------------------------
		ID3D12Resource* GetResource(Handle _handle) const;

		const std::string& GetName(Handle _handle) const;

		// -------------------------------------------------------------------------------
		// @brief	TransientResourceにのみ対応（ImportResourceに対してはnullptrが返る）
		// -------------------------------------------------------------------------------
		RHI::DescriptorHandle* GetRTV(Handle _handle)const;
		RHI::DescriptorHandle* GetSRV(Handle _handle)const;

		// -------------------------------------------------------------------------------
		// @brief	フレーム末にリセットする
		// -------------------------------------------------------------------------------
		void Clear();

	private:

		struct Entry
		{
			std::string		name;
			ID3D12Resource* pResource = nullptr;

			// TransientResourceのみ使用
			RHI::DescriptorHandle*	pRTVHandle	= nullptr;
			RHI::DescriptorHandle*	pSRVHandle	= nullptr;
			RHI::DescriptorPool*	pRTVPool	= nullptr;
			RHI::DescriptorPool*	pSRVPool	= nullptr;
		};

		std::vector<Entry> m_Resources;
	};
}
