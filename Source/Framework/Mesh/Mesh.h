#pragma once

// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------


// -------------------------------------------------------------------------------
// MeshVertex structure
// -------------------------------------------------------------------------------
struct MeshVertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexCoord;
	DirectX::XMFLOAT3 Tangent;

	MeshVertex() = default;

	MeshVertex(
		DirectX::XMFLOAT3 const& _position,
		DirectX::XMFLOAT3 const& _normal,
		DirectX::XMFLOAT2 const& _texCoord,
		DirectX::XMFLOAT3 const& _tangent)
		:	Position	(_position)
		,	Normal		(_normal)
		,	TexCoord	(_texCoord)
		,	Tangent		(_tangent)
	{/* DO_NOTHING */ }
	
	static const D3D12_INPUT_LAYOUT_DESC InputLayout;

private:

	static const int InputElementCount = 4;
	static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];

};

// -------------------------------------------------------------------------------
// Material structure
// -------------------------------------------------------------------------------
struct Material
{
	DirectX::XMFLOAT3	Diffuse;	// 拡散反射成分
	DirectX::XMFLOAT3	Specular;	// 鏡面反射成分
	float				Alpha;		// 透過成分
	float				Shininess;	// 鏡面反射強度
	std::string			DiffuseMap;	// テクスチャファイルパス
};

// -------------------------------------------------------------------------------
// Mesh structure
// -------------------------------------------------------------------------------
struct Mesh
{
	std::vector<MeshVertex> Vertices;	// 頂点データ
	std::vector<uint32_t>	Indices;	// 頂点インデックス
	uint32_t				MaterialId;	// マテリアル番号
};

// -------------------------------------------------------------------------------
// @brief	メッシュをロードする
// 
// @param[in]		filename		ファイルパス
// @param[out]		meshes			メッシュの格納先
// @param[out]		materials		マテリアルの格納先
// @retval true		ロードに成功
// @retval false	ロードに失敗
// -------------------------------------------------------------------------------
bool LoadMesh(
	const wchar_t*			_filename,
	std::vector<Mesh>&		_meshes,
	std::vector<Material>&	_materials);
