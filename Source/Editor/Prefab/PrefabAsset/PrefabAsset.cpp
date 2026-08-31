// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "PrefabAsset.h"

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
	constexpr const char* kFormatName	= "DX12FrameworkPrefab";
	constexpr int		  kFormatVersion = 1;

	// コンポーネントの型名を入れるキー
	// 値と同じ階層に置き、ファイルを開いたときに何のコンポーネントか一目で分かるようにする
	constexpr const char* kTypeKey = "Type";
}

// -------------------------------------------------------------------------------
// 書き出し
// -------------------------------------------------------------------------------
std::string Editor::SerializePrefab(const PrefabAsset& _prefab)
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
bool Editor::DeserializePrefab(std::string_view _json, PrefabAsset& _outPrefab)
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
