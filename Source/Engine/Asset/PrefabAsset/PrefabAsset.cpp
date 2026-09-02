// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "PrefabAsset.h"

#include <Engine/Asset/AssetTextFile/AssetTextFile.h>
#include <Engine/GameObject/GameObject.h>
#include <Engine/GameObject/GameObjectManager.h>
#include <Engine/GameObject/GameObjectFactory/GameObjectFactory.h>
#include <Engine/GameObject/ComponentRegistry/ComponentRegistry.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
// .prefabファイルの読み書き
//
// 形式は.effectと同じくJSON
// エディタ内で扱うテキストアセットの形をそろえ、読む側の手間を増やさないため
//
//	{
//	    "Format"     : "DX12FrameworkPrefab",
//	    "Version"    : 1,
//	    "Name"       : "Player",
//	    "Active"     : true,
//	    "Components" : [
//	        { "Type": "TransformComponent", "Position": [0,0,0], ... }
//	    ]
//	}
// -------------------------------------------------------------------------------
namespace
{
	constexpr const char* kFormatName		= "DX12FrameworkPrefab";
	constexpr int		  kFormatVersion	= 1;

	// コンポーネントの型名を入れるキー
	// 値と同じ階層に置き、ファイルを開いたときに何のコンポーネントか一目で分かるようにする
	constexpr const char* kTypeKey = "Type";
}

// -------------------------------------------------------------------------------
// 書き出し
// -------------------------------------------------------------------------------
std::string PrefabIO::Serialize(const PrefabAsset& _prefab)
{
	nlohmann::json json;

	// 形式名と版数を先頭に置く
	// 別のJSONを間違って開いたときに弾けるようにするため
	json["Format"]	= kFormatName;
	json["Version"]	= kFormatVersion;
	json["Name"]	= _prefab.Name;
	json["Active"]	= _prefab.Active;

	nlohmann::json components = nlohmann::json::array();

	for (const PrefabComponentData& component : _prefab.Components)
	{
		// 値をそのまま土台にして、型名を書き足す
		nlohmann::json entry = component.Values.is_object()
			? component.Values
			: nlohmann::json::object();

		entry[kTypeKey] = component.TypeName;

		components.emplace_back(std::move(entry));
	}

	json["Components"] = std::move(components);

	// インデント付きで書き出し、テキストとして読めるようにする
	return json.dump(4);
}

// -------------------------------------------------------------------------------
// 読み込み
// -------------------------------------------------------------------------------
bool PrefabIO::Deserialize(std::string_view _json, PrefabAsset& _outPrefab)
{
	// 例外を投げない形で解析する。壊れたファイルでエディタが落ちないようにするため
	nlohmann::json json = nlohmann::json::parse(_json, nullptr, false);

	if (json.is_discarded() || !json.is_object())
	{
		return false;	// JSONとして壊れている
	}

	// 形式名が違うものは、別の用途のJSONとして読み込まない
	if (!json.contains("Format") || json["Format"] != kFormatName)
	{
		return false;
	}

	if (json.contains("Name") && json["Name"].is_string())
	{
		_outPrefab.Name = json["Name"].get<std::string>();
	}

	if (json.contains("Active") && json["Active"].is_boolean())
	{
		_outPrefab.Active = json["Active"].get<bool>();
	}

	_outPrefab.Components.clear();

	if (!json.contains("Components") || !json["Components"].is_array())
	{
		return true;	// コンポーネントが1つも無いプレファブも許す
	}

	for (const nlohmann::json& entry : json["Components"])
	{
		if (!entry.is_object() || !entry.contains(kTypeKey) || !entry[kTypeKey].is_string())
		{
			continue;	// 型名が無いものは復元しようがないため読み飛ばす
		}

		PrefabComponentData component;
		component.TypeName	= entry[kTypeKey].get<std::string>();
		component.Values	= entry;

		// 型名は値ではないため、値側からは取り除いておく
		component.Values.erase(kTypeKey);

		_outPrefab.Components.emplace_back(std::move(component));
	}

	return true;
}

// -------------------------------------------------------------------------------
// ファイルへ保存
// -------------------------------------------------------------------------------
bool PrefabIO::SaveToFile(const std::filesystem::path& _path, const PrefabAsset& _prefab)
{
	return AssetTextFile::Write(_path, Serialize(_prefab));
}

// -------------------------------------------------------------------------------
// ファイルから読み込み
// -------------------------------------------------------------------------------
bool PrefabIO::LoadFromFile(const std::filesystem::path& _path, PrefabAsset& _outPrefab)
{
	std::string text;

	if (!AssetTextFile::Read(_path, text))
	{
		ELOG("PrefabIO::LoadFromFile() failed to open : %s", _path.string().c_str());
		return false;
	}

	if (!Deserialize(text, _outPrefab))
	{
		ELOG("PrefabIO::LoadFromFile() invalid prefab file : %s", _path.string().c_str());
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------
// シーン上のオブジェクトから設計図を作る
// -------------------------------------------------------------------------------
PrefabAsset PrefabIO::CaptureFromObject(const GameObject& _object)
{
	PrefabAsset prefab;

	prefab.Name		= _object.GetName();
	prefab.Active	= _object.IsActive();

	// -------------------------------------------------------------------------------
	// 登録済みのコンポーネントを、表の順に写し取る
	//
	// 写し取り方はComponentRegistryが持つため、
	// 複製・コンポーネント削除の取り消しと必ず同じ結果になる
	// -------------------------------------------------------------------------------
	for (const ComponentTypeInfo& info : ComponentRegistry::GetAll())
	{
		if (info.Has == nullptr || !info.Has(_object))
		{
			continue;
		}

		PrefabComponentData component;
		component.TypeName	= std::string(info.TypeName);
		component.Values	= ComponentRegistry::CaptureComponent(_object, info);

		prefab.Components.emplace_back(std::move(component));
	}

	return prefab;
}

// -------------------------------------------------------------------------------
// 設計図からオブジェクトを組み立てる
// -------------------------------------------------------------------------------
std::unique_ptr<GameObject> PrefabIO::Build(
	const GameObjectManager&	_objects,
	const PrefabAsset&			_prefab)
{
	// 名前が衝突しないよう、シーンの状況を見て連番を足す
	auto pObject = std::make_unique<GameObject>(
		GameObjectFactory::MakeUniqueName(_objects, _prefab.Name));

	pObject->SetActive(_prefab.Active);

	for (const PrefabComponentData& component : _prefab.Components)
	{
		const ComponentTypeInfo* pInfo = ComponentRegistry::Find(component.TypeName);

		if (pInfo == nullptr)
		{
			// 知らない型は読み飛ばす
			// 保存したときより登録が減っていても、残りは復元できるようにするため
			DLOG("PrefabIO::Build() unknown component type : %s", component.TypeName.c_str());
			continue;
		}

		ComponentRegistry::RestoreComponent(*pObject, *pInfo, component.Values);
	}

	return pObject;
}

// -------------------------------------------------------------------------------
// プレファブファイルかどうか
// -------------------------------------------------------------------------------
bool PrefabIO::IsPrefabPath(const std::filesystem::path& _path)
{
	return AssetTextFile::HasExtension(_path, kExtension);
}
