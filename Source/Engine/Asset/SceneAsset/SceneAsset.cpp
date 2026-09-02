// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "SceneAsset.h"

#include <Engine/Asset/AssetTextFile/AssetTextFile.h>
#include <Engine/GameObject/GameObject.h>
#include <Engine/GameObject/GameObjectManager.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
// .sceneファイルの読み書き
//
//	{
//	    "Format"  : "DX12FrameworkScene",
//	    "Version" : 1,
//	    "Name"    : "Stage01",
//	    "Objects" : [
//	        { "Name": "Player", "Active": true, "Components": [ ... ] },
//	        ...
//	    ]
//	}
//
// 1体分の中身はプレファブとまったく同じ形にしてある
// 「シーンから1体だけ取り出してプレファブにする」ことが、
// そのままコピーで済むようにするため
// -------------------------------------------------------------------------------
namespace
{
	constexpr const char* kFormatName		= "DX12FrameworkScene";
	constexpr int		  kFormatVersion	= 1;

	// -------------------------------------------------------------------------------
	// オブジェクト1体分をJSONへ / JSONから
	//
	// PrefabIO::Serialize は「1ファイル分」の形（Format行を含む）を作るため、
	// シーンの中へ並べる用にはここで中身だけを組み立てる
	// -------------------------------------------------------------------------------
	nlohmann::json ObjectToJson(const PrefabAsset& _object)
	{
		nlohmann::json json;

		json["Name"]	= _object.Name;
		json["Active"]	= _object.Active;

		nlohmann::json components = nlohmann::json::array();

		for (const PrefabComponentData& component : _object.Components)
		{
			nlohmann::json entry = component.Values.is_object()
				? component.Values
				: nlohmann::json::object();

			entry["Type"] = component.TypeName;

			components.emplace_back(std::move(entry));
		}

		json["Components"] = std::move(components);

		return json;
	}

	bool ObjectFromJson(const nlohmann::json& _json, PrefabAsset& _outObject)
	{
		if (!_json.is_object())
		{
			return false;
		}

		if (_json.contains("Name") && _json["Name"].is_string())
		{
			_outObject.Name = _json["Name"].get<std::string>();
		}

		if (_json.contains("Active") && _json["Active"].is_boolean())
		{
			_outObject.Active = _json["Active"].get<bool>();
		}

		if (!_json.contains("Components") || !_json["Components"].is_array())
		{
			return true;	// コンポーネントを持たないオブジェクトも許す
		}

		for (const nlohmann::json& entry : _json["Components"])
		{
			if (!entry.is_object() || !entry.contains("Type") || !entry["Type"].is_string())
			{
				continue;	// 型名が無いものは復元しようがない
			}

			PrefabComponentData component;
			component.TypeName	= entry["Type"].get<std::string>();
			component.Values	= entry;
			component.Values.erase("Type");

			_outObject.Components.emplace_back(std::move(component));
		}

		return true;
	}
}

// -------------------------------------------------------------------------------
// 書き出し
// -------------------------------------------------------------------------------
std::string SceneIO::Serialize(const SceneAsset& _scene)
{
	nlohmann::json json;

	json["Format"]	= kFormatName;
	json["Version"]	= kFormatVersion;
	json["Name"]	= _scene.Name;

	nlohmann::json objects = nlohmann::json::array();

	for (const PrefabAsset& object : _scene.Objects)
	{
		objects.emplace_back(ObjectToJson(object));
	}

	json["Objects"] = std::move(objects);

	return json.dump(4);
}

// -------------------------------------------------------------------------------
// 読み込み
// -------------------------------------------------------------------------------
bool SceneIO::Deserialize(std::string_view _json, SceneAsset& _outScene)
{
	nlohmann::json json = nlohmann::json::parse(_json, nullptr, false);

	if (json.is_discarded() || !json.is_object())
	{
		return false;	// JSONとして壊れている
	}

	// 別の用途のJSONを間違って開かないように、形式名を確かめる
	if (!json.contains("Format") || json["Format"] != kFormatName)
	{
		return false;
	}

	if (json.contains("Name") && json["Name"].is_string())
	{
		_outScene.Name = json["Name"].get<std::string>();
	}

	_outScene.Objects.clear();

	if (!json.contains("Objects") || !json["Objects"].is_array())
	{
		return true;	// 空のシーンも許す
	}

	for (const nlohmann::json& entry : json["Objects"])
	{
		PrefabAsset object;

		if (ObjectFromJson(entry, object))
		{
			_outScene.Objects.emplace_back(std::move(object));
		}
	}

	return true;
}

// -------------------------------------------------------------------------------
// ファイルへ保存
// -------------------------------------------------------------------------------
bool SceneIO::SaveToFile(const std::filesystem::path& _path, const SceneAsset& _scene)
{
	return AssetTextFile::Write(_path, Serialize(_scene));
}

// -------------------------------------------------------------------------------
// ファイルから読み込み
// -------------------------------------------------------------------------------
bool SceneIO::LoadFromFile(const std::filesystem::path& _path, SceneAsset& _outScene)
{
	std::string text;

	if (!AssetTextFile::Read(_path, text))
	{
		ELOG("SceneIO::LoadFromFile() failed to open : %s", _path.string().c_str());
		return false;
	}

	if (!Deserialize(text, _outScene))
	{
		ELOG("SceneIO::LoadFromFile() invalid scene file : %s", _path.string().c_str());
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------
// 今シーンに置いてあるものを写し取る
// -------------------------------------------------------------------------------
SceneAsset SceneIO::Capture(const GameObjectManager& _objects, std::string_view _name)
{
	SceneAsset scene;
	scene.Name = std::string(_name);

	scene.Objects.reserve(_objects.GetObjects().size());

	for (const auto& object : _objects.GetObjects())
	{
		if (object == nullptr)
		{
			continue;
		}

		// 1体分の写し取りはプレファブとまったく同じ手順を使う
		scene.Objects.emplace_back(PrefabIO::CaptureFromObject(*object));
	}

	return scene;
}

// -------------------------------------------------------------------------------
// シーンの中身を置き換える
// -------------------------------------------------------------------------------
size_t SceneIO::Apply(GameObjectManager& _objects, const SceneAsset& _scene)
{
	// -------------------------------------------------------------------------------
	// 今あるものをすべて消してから置き直す
	//
	// 「開く」操作なので、元の中身は残さない
	// 消してから作ることで、名前の連番も保存したときのまま復元される
	// -------------------------------------------------------------------------------
	_objects.Clear();

	size_t created = 0;

	for (const PrefabAsset& object : _scene.Objects)
	{
		auto pObject = PrefabIO::Build(_objects, object);

		if (pObject == nullptr)
		{
			continue;
		}

		_objects.AddExisting(std::move(pObject));
		++created;
	}

	DLOG("SceneIO::Apply() %s : %zu objects", _scene.Name.c_str(), created);

	return created;
}

// -------------------------------------------------------------------------------
// シーンファイルかどうか
// -------------------------------------------------------------------------------
bool SceneIO::IsScenePath(const std::filesystem::path& _path)
{
	return AssetTextFile::HasExtension(_path, kExtension);
}
