#pragma once

// -------------------------------------------------------------------------------
// RootSignatureLayout class
// 
// 概要 : 
//	JSONファイルからRootSignatureの構造（レイアウト）を読み
//	ID3D12RootSignatureの生成と「名前 → スロット」の対応表構築を行うクラス
// 
//	MeshComponent等はGetSlot("Transform")のように名前でスロット番号を取得する
// -------------------------------------------------------------------------------
class RootSignatureLayout
{
public:

	// -------------------------------------------------------------------------------
	// コンストラクタ
	// -------------------------------------------------------------------------------
	RootSignatureLayout();

	// -------------------------------------------------------------------------------
	// デストラクタ
	// -------------------------------------------------------------------------------
	~RootSignatureLayout();

	// -------------------------------------------------------------------------------
	// @brief	JSONファイルからRootSignatureを生成する
	// 
	// @param[in]	_pDevice		デバイス
	// @param[in]	_jsonPath		JSONファイルパス
	// @retval	true	成功
	// @retval	false	失敗
	// -------------------------------------------------------------------------------
	bool LoadFromJson(ID3D12Device* _pDevice, const std::wstring& _jsonPath);

	// -------------------------------------------------------------------------------
	// @brief	名前からスロット番号を取得する
	//			見つからない場合はログを出してUINT32_MAXを返す
	// -------------------------------------------------------------------------------
	uint32_t GetSlot(const std::string& _name) const;

	// -------------------------------------------------------------------------------
	// @brief	生成済みRootSignatureを取得する
	// -------------------------------------------------------------------------------
	ID3D12RootSignature* GetRootSignature() const;

private:

	ComPtr<ID3D12RootSignature>					m_pRootSignature;	// 生成済みRootSignature
	std::unordered_map<std::string, uint32_t>	m_SlotMap;			// パラメータ名 → スロット番号の対応表

	RootSignatureLayout	(const RootSignatureLayout&) = delete;
	void operator =		(const RootSignatureLayout&) = delete;

};
