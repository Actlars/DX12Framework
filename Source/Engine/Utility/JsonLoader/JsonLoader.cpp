// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "JsonLoader.h"
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
//		JSONファイルを読み込んでパースする
// -------------------------------------------------------------------------------
bool JsonLoader::Load(const std::wstring& _path, nlohmann::json& _outJson)
{
	std::ifstream ifs(_path);
	if (!ifs.is_open())
	{
		ELOG("JsonLoader::Load() : failed to open %ls", _path.c_str());
		return false;
	}

	try
	{
		ifs >> _outJson;
	}
	catch(const std::exception& e)
	{
		ELOG("JsonLoader::Load() : parse exception : %s(path = %ls)", e.what(), _path.c_str());
		return false;
	}

	return true;
}
