// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "WindowSettings.h"
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
// ウィンドウ状態の保存形式
//
//	{
//	    "PositionX" : 100,
//	    "PositionY" : 100,
//	    "Width"     : 1440,
//	    "Height"    : 810,
//	    "Maximized" : false
//	}
// -------------------------------------------------------------------------------
namespace
{
	// キーが無い、または型が違う場合は既定値のままにする
	// 項目が増えても古い設定ファイルがそのまま読めるようにするため
	int32_t ReadInt(const nlohmann::json& _json, const char* _key, int32_t _default)
	{
		if (!_json.contains(_key) || !_json.at(_key).is_number_integer())
		{
			return _default;
		}

		return _json.at(_key).get<int32_t>();
	}

	bool ReadBool(const nlohmann::json& _json, const char* _key, bool _default)
	{
		if (!_json.contains(_key) || !_json.at(_key).is_boolean())
		{
			return _default;
		}

		return _json.at(_key).get<bool>();
	}
}

// -------------------------------------------------------------------------------
// 読み込み
// -------------------------------------------------------------------------------
bool WindowSettings::Load(const std::filesystem::path& _path)
{
	std::error_code error;

	// 初回起動ではファイルが無いのが正常なので、ログも出さずに戻る
	if (!std::filesystem::exists(_path, error) || error)
	{
		return false;
	}

	std::ifstream file(_path, std::ios::binary);
	if (!file.is_open())
	{
		return false;
	}

	// 波かっこで初期化する
	// 丸かっこで書くと「イテレータを2つ取る関数の宣言」と解釈されてしまう
	const std::string text{
		std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>() };

	// 例外を投げない形で解析する。壊れたファイルで起動できなくならないようにするため
	const nlohmann::json json = nlohmann::json::parse(text, nullptr, false);

	if (json.is_discarded() || !json.is_object())
	{
		ELOG("WindowSettings::Load() invalid file. using default size");
		return false;
	}

	PositionX	= ReadInt (json, "PositionX", PositionX);
	PositionY	= ReadInt (json, "PositionY", PositionY);
	Width		= ReadInt (json, "Width",	  Width);
	Height		= ReadInt (json, "Height",	  Height);
	Maximized	= ReadBool(json, "Maximized", Maximized);

	return IsValid();
}

// -------------------------------------------------------------------------------
// 書き出し
// -------------------------------------------------------------------------------
bool WindowSettings::Save(const std::filesystem::path& _path) const
{
	// -------------------------------------------------------------------------------
	// 置き場所のフォルダが無ければ作る
	//
	// リポジトリに含めないフォルダへ書くため、初回は必ず存在しない
	// -------------------------------------------------------------------------------
	const std::filesystem::path directory = _path.parent_path();

	if (!directory.empty())
	{
		std::error_code error;
		std::filesystem::create_directories(directory, error);

		if (error)
		{
			ELOG("WindowSettings::Save() failed to create directory");
			return false;
		}
	}

	nlohmann::json json;

	json["PositionX"]	= PositionX;
	json["PositionY"]	= PositionY;
	json["Width"]		= Width;
	json["Height"]		= Height;
	json["Maximized"]	= Maximized;

	std::ofstream file(_path, std::ios::binary | std::ios::trunc);
	if (!file.is_open())
	{
		ELOG("WindowSettings::Save() failed to open file");
		return false;
	}

	// インデント付きで書き出し、手で開いても読めるようにする
	file << json.dump(4);

	return true;
}
