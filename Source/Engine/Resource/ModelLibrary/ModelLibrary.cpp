// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "ModelLibrary.h"

#include <Engine/Mesh/MeshLoader/MeshLoader.h>
#include <Engine/RHI/Core/Device/Device.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

namespace
{
	// -------------------------------------------------------------------------------
	// パス文字列をそろえる
	//
	//	"Assets\Model\A.fbx" と "Assets/Model/A.fbx" は同じファイルを指すが、
	//	文字列としては別物になる
	//	区切りを "/" に統一しておくことで、キーとして比較できるようにする
	// -------------------------------------------------------------------------------
	std::string NormalizeSeparators(std::string _text)
	{
		std::replace(_text.begin(), _text.end(), '\\', '/');
		return _text;
	}
}

// -------------------------------------------------------------------------------
// 初期化
// -------------------------------------------------------------------------------
bool ModelLibrary::Init(RHI::Device* _pDevice, const std::filesystem::path& _projectRoot)
{
	if (_pDevice == nullptr)
	{
		ELOG("ModelLibrary::Init() device is null");
		return false;
	}

	m_pDevice		= _pDevice;
	m_ProjectRoot	= _projectRoot.lexically_normal();

	return true;
}

// -------------------------------------------------------------------------------
// 終了
// -------------------------------------------------------------------------------
void ModelLibrary::Term()
{
	// GPUリソースを持つため、デバイスより先に解放されるようにする
	// （所有者であるApplicationがその順序を守る）
	m_Models.clear();
	m_pDevice = nullptr;
}

// -------------------------------------------------------------------------------
// プロジェクトからの相対パス（キー）へ変換する
// -------------------------------------------------------------------------------
std::string ModelLibrary::ToKey(const std::filesystem::path& _path) const
{
	if (_path.empty())
	{
		return {};
	}

	std::filesystem::path absolute = ToAbsolute(_path);

	// プロジェクトの外にあるファイルは相対化できないため、絶対パスのまま扱う
	std::error_code error;
	std::filesystem::path relative = std::filesystem::relative(absolute, m_ProjectRoot, error);

	if (error || relative.empty() || relative.native().rfind(L"..", 0) == 0)
	{
		return NormalizeSeparators(absolute.string());
	}

	return NormalizeSeparators(relative.string());
}

// -------------------------------------------------------------------------------
// 絶対パスへ変換する
// -------------------------------------------------------------------------------
std::filesystem::path ModelLibrary::ToAbsolute(const std::filesystem::path& _path) const
{
	if (_path.empty())
	{
		return {};
	}

	if (_path.is_absolute())
	{
		return _path.lexically_normal();
	}

	return (m_ProjectRoot / _path).lexically_normal();
}

// -------------------------------------------------------------------------------
// 読み込み済みのモデルを探す
// -------------------------------------------------------------------------------
const ModelResource* ModelLibrary::Find(const std::filesystem::path& _path) const
{
	const auto it = m_Models.find(ToKey(_path));

	return (it != m_Models.end()) ? it->second.get() : nullptr;
}

// -------------------------------------------------------------------------------
// モデルを読み込む
// -------------------------------------------------------------------------------
const ModelResource* ModelLibrary::Load(const std::filesystem::path& _path)
{
	if (m_pDevice == nullptr || _path.empty())
	{
		return nullptr;
	}

	// -------------------------------------------------------------------------------
	// すでに読み込んでいればそれを返す
	//
	// 同じモデルを何体置いてもGPU上の実体は1つで済ませるための入り口
	// -------------------------------------------------------------------------------
	const std::string key = ToKey(_path);

	if (const auto it = m_Models.find(key); it != m_Models.end())
	{
		return it->second.get();
	}

	const std::filesystem::path absolute = ToAbsolute(_path);

	std::error_code error;
	if (!std::filesystem::exists(absolute, error) || error)
	{
		ELOG("ModelLibrary::Load() file not found : %s", absolute.string().c_str());
		return nullptr;
	}

	// -------------------------------------------------------------------------------
	// ファイルからCPU側のデータを読む
	// -------------------------------------------------------------------------------
	std::vector<ResMesh>		resMeshes;
	std::vector<ResMaterial>	resMaterials;

	if (!MeshLoader::Load(absolute.wstring(), resMeshes, resMaterials))
	{
		ELOG("ModelLibrary::Load() MeshLoader::Load failed : %s", absolute.string().c_str());
		return nullptr;
	}

	auto model		= std::make_unique<ModelResource>();
	model->m_Path	= absolute;
	model->m_Key	= key;

	// -------------------------------------------------------------------------------
	// GPUリソースを作る
	//
	// 途中で失敗した場合は、ここまでに作ったものごと捨てる
	// 半端に読み込まれたモデルを表に載せないため
	// -------------------------------------------------------------------------------
	auto* pD3DDevice = m_pDevice->GetDevice();

	model->m_Meshes.reserve(resMeshes.size());
	for (const auto& resMesh : resMeshes)
	{
		auto mesh = std::make_unique<Mesh>();

		if (!mesh->Init(pD3DDevice, resMesh))
		{
			ELOG("ModelLibrary::Load() Mesh::Init failed : %s", absolute.string().c_str());
			return nullptr;
		}

		model->m_Meshes.emplace_back(std::move(mesh));
	}

	model->m_Materials.reserve(resMaterials.size());
	for (const auto& resMaterial : resMaterials)
	{
		auto material = std::make_unique<Material>();

		if (!material->Init(m_pDevice, resMaterial))
		{
			ELOG("ModelLibrary::Load() Material::Init failed : %s", absolute.string().c_str());
			return nullptr;
		}

		model->m_Materials.emplace_back(std::move(material));
	}

	// -------------------------------------------------------------------------------
	// メッシュとマテリアルを組にして並べる
	//
	// 使う側が「何番目」で指せるよう、ここで対応付けを済ませておく
	// -------------------------------------------------------------------------------
	model->m_Parts.reserve(model->m_Meshes.size());

	for (size_t i = 0; i < model->m_Meshes.size(); ++i)
	{
		const auto materialId = model->m_Meshes[i]->GetMaterialId();

		ModelPart part;
		part.pMesh		= model->m_Meshes[i].get();
		part.pMaterial	= (materialId < model->m_Materials.size())
			? model->m_Materials[materialId].get()
			: nullptr;
		part.Name		= "Part " + std::to_string(i);

		model->m_Parts.emplace_back(std::move(part));
	}

	DLOG("ModelLibrary : loaded %s (parts=%zu)", key.c_str(), model->m_Parts.size());

	const ModelResource* pResult = model.get();
	m_Models.emplace(key, std::move(model));

	return pResult;
}

// -------------------------------------------------------------------------------
// 読み込み済みモデルの一覧
// -------------------------------------------------------------------------------
std::vector<std::string> ModelLibrary::GetLoadedKeys() const
{
	std::vector<std::string> keys;
	keys.reserve(m_Models.size());

	for (const auto& [key, model] : m_Models)
	{
		keys.emplace_back(key);
	}

	// 表示順が毎回変わらないように並べておく
	std::sort(keys.begin(), keys.end());

	return keys;
}
