// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "AssetTextFile.h"
#include <Engine/Utility/Debug/Logger/Logger.h>

// -------------------------------------------------------------------------------
// ファイル全体を文字列として読む
// -------------------------------------------------------------------------------
bool AssetTextFile::Read(const std::filesystem::path& _path, std::string& _outText)
{
	// binaryで開くのは、改行を勝手に変換させないため
	// JSONとして読むだけなので、そのままのバイト列でよい
	std::ifstream file(_path, std::ios::binary);

	if (!file.is_open())
	{
		return false;
	}

	_outText.assign(
		std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>());

	return true;
}

// -------------------------------------------------------------------------------
// 文字列をファイルへ書き出す
// -------------------------------------------------------------------------------
bool AssetTextFile::Write(const std::filesystem::path& _path, std::string_view _text)
{
	// -------------------------------------------------------------------------------
	// 置き場所のフォルダが無ければ作る
	//
	// 保存先を新しく決めたときに、フォルダが無いだけで失敗しないようにする
	// -------------------------------------------------------------------------------
	const std::filesystem::path directory = _path.parent_path();

	if (!directory.empty())
	{
		std::error_code error;
		std::filesystem::create_directories(directory, error);

		if (error)
		{
			ELOG("AssetTextFile::Write() failed to create directory : %s", directory.string().c_str());
			return false;
		}
	}

	std::ofstream file(_path, std::ios::binary | std::ios::trunc);

	if (!file.is_open())
	{
		ELOG("AssetTextFile::Write() failed to open : %s", _path.string().c_str());
		return false;
	}

	file.write(_text.data(), static_cast<std::streamsize>(_text.size()));

	return true;
}

// -------------------------------------------------------------------------------
// 拡張子の判定（大文字小文字は無視する）
// -------------------------------------------------------------------------------
bool AssetTextFile::HasExtension(const std::filesystem::path& _path, std::string_view _extension)
{
	if (!_path.has_extension())
	{
		return false;
	}

	std::string extension = _path.extension().string();

	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](unsigned char _c) { return static_cast<char>(std::tolower(_c)); });

	return extension == _extension;
}
