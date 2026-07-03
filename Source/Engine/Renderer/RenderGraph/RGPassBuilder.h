#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "RGTypes.h"

namespace RG
{
	// -------------------------------------------------------------------------------
	// RGPassBuilder class
	// 
	// 概要 : 
	//	各パスSetupフェーズで、そのパスが使うリソースと必要なステートを宣言する
	//	（各パスのSetupフェーズで、何を読むか、何を書くかを宣言する）
	//	ここで宣言された情報をもとに、RenderGraphが実行直前のバリアを自動生成する
	// -------------------------------------------------------------------------------
	class PassBuilder
	{
	public:

		struct ResourceUsage
		{
			Handle					resourceHandle;
			D3D12_RESOURCE_STATES	requiredState;
		};

		// -------------------------------------------------------------------------------
		// @brief	このパスが指定ステートで利用するリソースを宣言する
		//			読み取り専用でも書き込みでも、必要なステートをそのまま渡せばよい
		//			（Read/Writeはわけていない）
		// -------------------------------------------------------------------------------
		void Use(Handle _handle, D3D12_RESOURCE_STATES _state) 
		{ m_Usages.push_back({ _handle, _state }); }

		// @brief	リソースの使用用途を返す
		const std::vector<ResourceUsage>& GetUsages() const { return m_Usages; }

	private:

		std::vector<ResourceUsage> m_Usages;

	};
}
