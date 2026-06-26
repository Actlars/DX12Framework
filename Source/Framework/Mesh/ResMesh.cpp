//// -------------------------------------------------------------------------------
//// Includes
//// -------------------------------------------------------------------------------
//#include "Mesh.h"
//
//namespace {
//
//// -------------------------------------------------------------------------------
////	MeshLoader class
//// -------------------------------------------------------------------------------
//class MeshLoader
//{
//	// -------------------------------------------------------------------------------
//	// list of friend classes and methods
//	// -------------------------------------------------------------------------------
//	/* NOTHING */
//
//public:
//
//	// -------------------------------------------------------------------------------
//	// UTF-8文字列に変換
//	// -------------------------------------------------------------------------------
//	std::string ToUTF8(const std::wstring& _value)
//	{
//		auto length = WideCharToMultiByte(
//			CP_UTF8, 0U, _value.data(), -1, nullptr, 0, nullptr, nullptr);
//		auto buffer = new char[length];
//
//		WideCharToMultiByte(
//			CP_UTF8, 0U, _value.data(), -1, buffer, length, nullptr, nullptr);
//
//		std::string result(buffer);
//		delete[] buffer;
//		buffer = nullptr;
//
//		return result;
//	}
//
//	// -------------------------------------------------------------------------------
//	// public variables
//	// -------------------------------------------------------------------------------
//	/* NOTHING */
//
//	// -------------------------------------------------------------------------------
//	// public methods
//	// -------------------------------------------------------------------------------
//	MeshLoader();
//	~MeshLoader();
//
//	bool Load(
//		const wchar_t* _filename,
//		std::vector<Mesh>& _meshs,
//		std::vector<Material>& _materials);
//
//private:
//
//	// -------------------------------------------------------------------------------
//	// private variables
//	// -------------------------------------------------------------------------------
//	/* NOTHING */
//
//	// -------------------------------------------------------------------------------
//	// private methods
//	// -------------------------------------------------------------------------------
//	void ParseMesh(Mesh& _dstMesh, const aiMesh* _pSrcMesh);
//	void ParseMaterial(Material& _dstMaterial, const aiMaterial* _pSrcMaterial);
//
//};
//
//// -------------------------------------------------------------------------------
//// コンストラクタ
//// -------------------------------------------------------------------------------
//MeshLoader::MeshLoader()
//{ /* DO_NOTHING */
//}
//
//// -------------------------------------------------------------------------------
//// デストラクタ
//// -------------------------------------------------------------------------------
//MeshLoader::~MeshLoader()
//{ /* DO_NOTHING */
//}
//
//// -------------------------------------------------------------------------------
//// メッシュをロード
//// -------------------------------------------------------------------------------
//bool MeshLoader::Load
//(
//	const wchar_t* _filename,
//	std::vector<Mesh>& _meshes,
//	std::vector<Material>& _materials
//)
//{
//	if (_filename == nullptr)
//	{
//		return false;
//	}
//
//	// wchar_tからchar型（UTF-8）に変換
//	auto path = ToUTF8(_filename);
//
//	Assimp::Importer importer;
//	int flag = 0;
//	flag |= aiProcess_Triangulate;
//	flag |= aiProcess_PreTransformVertices;
//	flag |= aiProcess_CalcTangentSpace;
//	flag |= aiProcess_GenSmoothNormals;
//	flag |= aiProcess_GenUVCoords;
//	flag |= aiProcess_RemoveRedundantMaterials;
//	flag |= aiProcess_OptimizeMeshes;
//
//	// ファイルを読み込み
//	auto pScene = importer.ReadFile(path, flag);
//
//	// チェック
//	if (pScene == nullptr)
//	{
//		return false;
//	}
//
//	// メッシュメモリを確保
//	_meshes.clear();
//	_meshes.resize(pScene->mNumMeshes);
//
//	// メッシュデータを変換
//	for (size_t i = 0; i < _meshes.size(); ++i)
//	{
//		const auto pMesh = pScene->mMeshes[i];
//		ParseMesh(_meshes[i], pMesh);
//	}
//
//	// マテリアルのメモリを確保
//	_materials.clear();
//	_materials.resize(pScene->mNumMaterials);
//
//	// マテリアルデータを変換
//	for (size_t i = 0; i < _materials.size(); ++i)
//	{
//		const auto pMaterial = pScene->mMaterials[i];
//		ParseMaterial(_materials[i], pMaterial);
//	}
//
//	// 不要になったのでクリア
//	pScene = nullptr;
//
//	// 正常終了
//	return true;
//}
//
//// -------------------------------------------------------------------------------
//// メッシュデータを解析
//// -------------------------------------------------------------------------------
//void MeshLoader::ParseMesh(Mesh& _dstMesh, const aiMesh* _pSrcMesh)
//{
//	// マテリアル番号を設定
//	_dstMesh.MaterialId = _pSrcMesh->mMaterialIndex;
//
//	aiVector3D zero3D(0.0f, 0.0f, 0.0f);
//
//	// 頂点データのメモリを確保
//	_dstMesh.Vertices.resize(_pSrcMesh->mNumVertices);
//
//	for (auto i = 0u; i < _pSrcMesh->mNumVertices; ++i)
//	{
//		auto pPosition = &(_pSrcMesh->mVertices[i]);
//		auto pNormal = &(_pSrcMesh->mNormals[i]);
//		auto pTexCoord = (_pSrcMesh->HasTextureCoords(0)) ? &(_pSrcMesh->mTextureCoords[0][i]) : &zero3D;
//		auto pTangent = (_pSrcMesh->HasTangentsAndBitangents()) ? &(_pSrcMesh->mTangents[i]) : &zero3D;
//
//		_dstMesh.Vertices[i] = MeshVertex(
//			DirectX::XMFLOAT3(pPosition->x, pPosition->y, pPosition->z),
//			DirectX::XMFLOAT3(pNormal->x, pNormal->y, pNormal->z),
//			DirectX::XMFLOAT2(pTexCoord->x, pTexCoord->y),
//			DirectX::XMFLOAT3(pTangent->x, pTangent->y, pTangent->z)
//		);
//	}
//
//	// 頂点インデックスのメモリを確保
//	_dstMesh.Indices.resize(_pSrcMesh->mNumFaces * 3);
//
//	for (auto i = 0u; i < _pSrcMesh->mNumFaces; ++i)
//	{
//		const auto& face = _pSrcMesh->mFaces[i];
//		assert(face.mNumIndices == 3);	// 三角形化しているので必ず３になっている
//
//		_dstMesh.Indices[i * 3 + 0] = face.mIndices[0];
//		_dstMesh.Indices[i * 3 + 1] = face.mIndices[1];
//		_dstMesh.Indices[i * 3 + 2] = face.mIndices[2];
//	}
//}
//
//// -------------------------------------------------------------------------------
//// マテリアルデータを解析
//// -------------------------------------------------------------------------------
//void MeshLoader::ParseMaterial(Material& _dstMaterial, const aiMaterial* _pSrcMaterial)
//{
//	// 拡散反射成分
//	{
//		aiColor3D color(0.0f, 0.0f, 0.0f);
//
//		if (_pSrcMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
//		{
//			_dstMaterial.Diffuse.x = color.r;
//			_dstMaterial.Diffuse.y = color.g;
//			_dstMaterial.Diffuse.z = color.b;
//		}
//		else
//		{
//			_dstMaterial.Diffuse.x = 0.5f;
//			_dstMaterial.Diffuse.y = 0.5f;
//			_dstMaterial.Diffuse.z = 0.5f;
//		}
//	}
//
//	// 鏡面反射成分
//	{
//		aiColor3D color(0.0f, 0.0f, 0.0f);
//
//		if (_pSrcMaterial->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
//		{
//			_dstMaterial.Specular.x = color.r;
//			_dstMaterial.Specular.y = color.g;
//			_dstMaterial.Specular.z = color.b;
//		}
//		else
//		{
//			_dstMaterial.Specular.x = 0.0f;
//			_dstMaterial.Specular.y = 0.0f;
//			_dstMaterial.Specular.z = 0.0f;
//		}
//	}
//
//	// 鏡面反射強度
//	{
//		auto shininess = 0.0f;
//		if (_pSrcMaterial->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
//		{
//			_dstMaterial.Shininess = shininess;
//		}
//		else
//		{
//			_dstMaterial.Shininess = 0.0f;
//		}
//	}
//
//	// ディヒューズマップ
//	{
//		aiString path;
//		if (_pSrcMaterial->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), path) == AI_SUCCESS)
//		{
//			_dstMaterial.DiffuseMap = std::string(path.C_Str());
//		}
//		else
//		{
//			_dstMaterial.DiffuseMap.clear();
//		}
//	}
//}
//}// namespace
//
//#define FMT_FLOAT3	DXGI_FORMAT_R32G32B32_FLOAT
//#define FMT_FLOAT2	DXGI_FORMAT_R32G32_FLOAT
//#define APPEND		D3D12_APPEND_ALIGNED_ELEMENT
//#define IL_VERTEX	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA
//
//const D3D12_INPUT_ELEMENT_DESC MeshVertex::InputElements[] = {
//	{"POSITION", 0, FMT_FLOAT3, 0, APPEND, IL_VERTEX, 0},
//	{"NORMAL", 0, FMT_FLOAT3,0,APPEND,IL_VERTEX,0},
//	{"TEXCOORD",0,FMT_FLOAT2,0,APPEND,IL_VERTEX,0},
//	{"TANGENT",0,FMT_FLOAT3,0,APPEND,IL_VERTEX,0}
//};
//const D3D12_INPUT_LAYOUT_DESC MeshVertex::InputLayout = {
//	MeshVertex::InputElements,
//	MeshVertex::InputElementCount
//};
//static_assert(sizeof(MeshVertex) == 44, "Vertex struct/layout mismatch");
//
//// -------------------------------------------------------------------------------
//// メッシュをロード
//// -------------------------------------------------------------------------------
//bool LoadMesh
//(
//	const wchar_t*			_filename,
//	std::vector<Mesh>&		_meshes,
//	std::vector<Material>&	_materials
//)
//{
//	MeshLoader loader;
//	return loader.Load(_filename, _meshes, _materials);
//}