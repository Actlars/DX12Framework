// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "MeshLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <Framework/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
// InputLayout定義（PolygonMeshResource のPSO 生成時に参照）
// -------------------------------------------------------------------------------
#define FMT3	DXGI_FORMAT_R32G32B32_FLOAT
#define FMT2	DXGI_FORMAT_R32G32_FLOAT
#define APPEND	D3D12_APPEND_ALIGNED_ELEMENT
#define VERTEX	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA

const D3D12_INPUT_ELEMENT_DESC ResMeshVertex::InputElements[]
=
{
	{"POSITION",	0, FMT3, 0, APPEND, VERTEX, 0},
	{"NORMAL",		0, FMT3, 0, APPEND, VERTEX, 0},
	{"TEXCOORD",	0, FMT2, 0, APPEND, VERTEX, 0},
	{"TANGENT",		0, FMT3, 0, APPEND, VERTEX, 0},
	{"BITANGENT",	0, FMT3, 0, APPEND, VERTEX, 0},
};

const D3D12_INPUT_LAYOUT_DESC ResMeshVertex::InputLayout =
{
	ResMeshVertex::InputElements,
	ResMeshVertex::InputElementCount
};

#undef FMT3
#undef FMT2
#undef APPEND
#undef VERTEX

namespace {
	// -------------------------------------------------------------------------------
	// wchar_t → UTF-8変換
	// AssimpのReadFileはchar*(UTF-8)を要求するため
	// -------------------------------------------------------------------------------
	std::string ToUTF8(const std::wstring& _value)
	{
		const int len = WideCharToMultiByte(
			CP_UTF8, 0, _value.data(), -1,
			nullptr, 0, nullptr, nullptr);

		std::string result(len, '\0');
		WideCharToMultiByte(
			CP_UTF8, 0, _value.data(), -1,
			result.data(), len, nullptr, nullptr);

		return result;
	}

	// -------------------------------------------------------------------------------
	// char*(UTF-8) → wstring変換
	// テクスチャパスをwstringで保持するために使う
	// -------------------------------------------------------------------------------
	std::wstring ToWString(const char* _value)
	{
		const int len = MultiByteToWideChar(
			CP_UTF8, 0, _value, -1, nullptr, 0);

		std::wstring result(len, L'\0');
		MultiByteToWideChar(
			CP_UTF8, 0, _value, -1, result.data(), len);

		return result;
	}

	// -------------------------------------------------------------------------------
	// aiMesh → ResMesh変換
	// -------------------------------------------------------------------------------
	void ParseMesh(ResMesh& _dst, const aiMesh* _pSrc)
	{
		_dst.MaterialId = _pSrc->mMaterialIndex;

		const aiVector3D zero(0.0f, 0.0f, 0.0f);

		// 頂点データ
		_dst.Vertices.resize(_pSrc->mNumVertices);

		for (auto i = 0u; i < _pSrc->mNumVertices; ++i)
		{
			const auto& pos = _pSrc->mVertices[i];
			const auto& n	= _pSrc->mNormals[i];

			// UV・接線・従法線は存在しない場合もゼロで補完
			const auto& uv		= _pSrc->HasTextureCoords(0)
				? _pSrc->mTextureCoords[0][i] : zero;
			const auto& tan		= _pSrc->HasTangentsAndBitangents()
				? _pSrc->mTangents[i] : zero;
			const auto& btan	= _pSrc->HasTangentsAndBitangents()
				? _pSrc->mBitangents[i] : zero;

			_dst.Vertices[i] = ResMeshVertex(
				{ pos.x,	pos.y,	pos.z },
				{ n.x,		n.y,	n.z },
				{ uv.x,		uv.y },
				{ tan.x,	tan.y,	tan.z },
				{ btan.x,	btan.y,	btan.z });
		}

		// インデックスデータ
		// aiProcess_Triangulateで三角形化済みなのでmNumIndices == 3が保証される
		_dst.Indices.reserve(_pSrc->mNumFaces * 3);
		_dst.Indices.resize	(_pSrc->mNumFaces * 3);

		for (auto i = 0u; i < _pSrc->mNumFaces; ++i)
		{
			const auto& face = _pSrc->mFaces[i];
			assert(face.mNumIndices == 3);

			_dst.Indices[i * 3 + 0] = face.mIndices[0];
			_dst.Indices[i * 3 + 1] = face.mIndices[1];
			_dst.Indices[i * 3 + 2] = face.mIndices[2];
		}
	}

	// -------------------------------------------------------------------------------
	// aiMaterial → ResMaterial変換
	// -------------------------------------------------------------------------------
	void ParseMaterial(
		ResMaterial& _dst,
		const aiMaterial* _pSrc,
		const std::wstring& _modelDir)
	{
		// 色パラメータ
		{
			aiColor3D c(0.5f, 0.5f, 0.5f);
			if (_pSrc->Get(AI_MATKEY_COLOR_DIFFUSE, c) == AI_SUCCESS)
			{ _dst.Diffuse = { c.r,c.g,c.b }; }
		}
		{
			aiColor3D c(0.0f, 0.0f, 0.0f);
			if (_pSrc->Get(AI_MATKEY_COLOR_SPECULAR, c) == AI_SUCCESS)
			{ _dst.Specular = { c.r,c.g,c.b }; }
		}
		{
			aiColor3D c(0.0f, 0.0f, 0.0f);
			if (_pSrc->Get(AI_MATKEY_COLOR_EMISSIVE, c) == AI_SUCCESS) 
			{ _dst.Emissive = { c.r,c.g,c.b }; }
		}
		{
			float v = 0.0f;
			if (_pSrc->Get(AI_MATKEY_SHININESS, v) == AI_SUCCESS) 
			{ _dst.Shininess = v; }
		}
		{
			float v = 1.0f;
			if (_pSrc->Get(AI_MATKEY_OPACITY, v) == AI_SUCCESS) 
			{ _dst.Alpha = v; }
		}

		// テクスチャパスの取得
		// Assimpが返すパスはモデルファイルからの相対パスなので
		// モデルのディレクトリと結合して使いやすい絶対パスに変換する
		auto resolveTexPath = [&](aiTextureType _type)->std::wstring
			{
				aiString path;
				if (_pSrc->GetTexture(_type, 0, &path) != AI_SUCCESS)
				{
					return L"";
				}

				// パス区切り文字をスラッシュに統一してから結合
				auto relPath = ToWString(path.C_Str());
				for (auto& c : relPath)
				{
					if (c == L'\\') { c = L'/'; }
				}

				return _modelDir + L"/" + relPath;
			};

		_dst.DiffuseMap		= resolveTexPath(aiTextureType_DIFFUSE);
		_dst.NormalMap		= resolveTexPath(aiTextureType_NORMALS);
		_dst.SpecularMap	= resolveTexPath(aiTextureType_SPECULAR);
		_dst.ShininessMap	= resolveTexPath(aiTextureType_SHININESS);
	}
} // namespace

// -------------------------------------------------------------------------------
// MeshLoader::Load
// -------------------------------------------------------------------------------
bool MeshLoader::Load(
	const std::wstring&			_path,
	std::vector<ResMesh>&		_meshes,
	std::vector<ResMaterial>&	_materials)
{
	// ファイル存在確認
	if (!std::filesystem::exists(_path))
	{
		ELOG("MEshLoader::Load() File not found.path = %ls", _path.c_str());
		return false; 
	}

	// Assimpでロード
	Assimp::Importer importer;

	const unsigned int flags =
			aiProcess_Triangulate				// 全ポリゴンを三角形に変換
		|	aiProcess_PreTransformVertices		// ノード階層をフラット化
		|	aiProcess_CalcTangentSpace			// 接線・従法線を自動計算
		|	aiProcess_GenSmoothNormals			// 法線がない場合に平滑法線を生成
		|	aiProcess_GenUVCoords				// UVがない場合に自動生成
		|	aiProcess_RemoveRedundantMaterials	// 重複マテリアルを削除
		|	aiProcess_OptimizeMeshes			// メッシュを最小化
		|	aiProcess_JoinIdenticalVertices;	// 重複頂点をマージしてインデックスを最適化
	
	const auto* pScene = importer.ReadFile(ToUTF8(_path), flags);
	if (pScene == nullptr)
	{
		ELOG("MeshLoader::Load() Assimp error : %s", importer.GetErrorString());
		return false;
	}

	// モデルのディレクトリを取得（テクスチャパス解決に使う）
	const auto modelDir = std::filesystem::path(_path)
		.parent_path().wstring();

	// メッシュの変換
	_meshes.clear();
	_meshes.resize(pScene->mNumMeshes);
	for (auto i = 0u; i < pScene->mNumMeshes; ++i)
	{ ParseMesh(_meshes[i], pScene->mMeshes[i]); }

	// マテリアルの変換
	_materials.clear();
	_materials.resize(pScene->mNumMaterials);
	for (auto i = 0u; i < pScene->mNumMaterials; ++i) 
	{ ParseMaterial(_materials[i], pScene->mMaterials[i], modelDir); }

	return true;
}
