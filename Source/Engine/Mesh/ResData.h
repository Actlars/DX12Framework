#pragma once
// -------------------------------------------------------------------------------
// Includess
// -------------------------------------------------------------------------------


// -------------------------------------------------------------------------------
// ResMeshVertex 構造体
// 
// 概要 : 
//	ファイルから読み込んだ頂点の生データ
//	GPU転送前の純粋なCPU側データ
// 
//	Bitangent（従法線）を含めることで法線マップ計算の精度を上げる
//	メッシュシェーダーへの移行時はこの構造体を起点に
//	メッシュレット用バッファを生成
// -------------------------------------------------------------------------------
struct ResMeshVertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexCoord;
	DirectX::XMFLOAT3 Tangent;
	DirectX::XMFLOAT3 Bitangent;

	ResMeshVertex() = default;

	ResMeshVertex(
		const DirectX::XMFLOAT3& _pos,
		const DirectX::XMFLOAT3& _normal,
		const DirectX::XMFLOAT2& _uv,
		const DirectX::XMFLOAT3& _tangent,
		const DirectX::XMFLOAT3& _bitangent)
		: Position	(_pos)
		, Normal	(_normal)
		, TexCoord	(_uv)
		, Tangent	(_tangent)
		, Bitangent	(_bitangent)
	{ /* DO_NOTHING */ }

	// PolygonMeshResourceのInputLayoutとして参照する
	// メッシュシェーダーはInputLayout不要なのでMeshResource側で判断
	static const D3D12_INPUT_LAYOUT_DESC InputLayout;

private:

	static const int InputElementCount = 5;
	static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];

};

// sizeofチェック : Position(12) + Normal(12) + TexCoord(8) + Tangent(12) + Bitangent(12) = 56
static_assert(sizeof(ResMeshVertex) == 56, "ResMeshVertex size mismatch");

// -------------------------------------------------------------------------------
// ResMesh構造体
// 
// 概要 : 
//	ファイルから読み込んだ1メッシュ分の生データ
//	GPUリソースは一切持たない
// -------------------------------------------------------------------------------
struct ResMesh
{
	std::vector<ResMeshVertex>	Vertices;		// 頂点データ
	std::vector<uint32_t>		Indices;		// インデックスデータ
	uint32_t					MaterialId = 0;	// 対応するマテリアルのインデックス
};

// -------------------------------------------------------------------------------
// ResMaterial構造体
// 
// 概要 : 
//	ファイルから読み込んだ1マテリアル分の生データ
//	GPUリソースは一切持たない
//	テクスチャはパス文字列だけ保持し、
//	実際のロードはMaterialクラスが行う
// 
//	パスがから文字列の場合はそのテクスチャが存在しないことを意味し、
//	Material::Init()側でダミーテクスチャを差し込む
// -------------------------------------------------------------------------------
struct ResMaterial
{
	DirectX::XMFLOAT3	Diffuse		= { 0.5f,0.5f,0.5f };	// 拡散反射色
	DirectX::XMFLOAT3	Specular	= { 0.0f,0.0f,0.0f };	// 鏡面反射色
	DirectX::XMFLOAT3	Emissive	= { 0.0f,0.0f,0.0f };	// 自己発光色
	float				Alpha		= 1.0f;					// 透明度（1.0 = 不透明）
	float				Shininess	= 0.0f;					// 鏡面反射の鋭さ
	std::wstring		DiffuseMap;							// ディヒューズテクスチャ（空 = なし）
	std::wstring		NormalMap;							// 法線マップパス
	std::wstring		SpecularMap;						// スペキュラーマップパス
	std::wstring		ShininessMap;						// シャイネスマップパス
};

