// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "AssetDatabase.h"
#include <Engine/Utility/Debug/Logger/Logger.h>

namespace
{
	// 拡張子は大文字小文字を区別せずに比較する
	std::string ToLower(std::string _value)
	{
		std::transform(_value.begin(), _value.end(), _value.begin(),
			[](unsigned char _ch) { return static_cast<char>(std::tolower(_ch)); });
		return _value;
	}
}

// -------------------------------------------------------------------------------
// 初期化
// -------------------------------------------------------------------------------
bool Editor::AssetDatabase::Init(const std::filesystem::path& _rootPath)
{
	std::error_code error;

	// ルートが無ければ作る。初回起動でもコンテンツブラウザが空で開けるようにする
	if (!std::filesystem::exists(_rootPath, error))
	{
		std::filesystem::create_directories(_rootPath, error);
		if (error)
		{
			ELOG("AssetDatabase::Init() failed to create root directory");
			return false;
		}
	}

	m_RootDirectory		= std::filesystem::absolute(_rootPath, error);
	m_CurrentDirectory	= m_RootDirectory;

	// ルート以下をまとめて監視する
	// 失敗しても手動の再読み込みは使えるため、致命的な扱いにはしない
	if (!m_Watcher.Start(m_RootDirectory, true))
	{
		ELOG("AssetDatabase::Init() failed to watch content directory (auto refresh disabled)");
	}

	Refresh();
	return true;
}

// -------------------------------------------------------------------------------
// 現在のフォルダの中身を読み直す
//
// 並び順は「フォルダが先、次に名前順」で固定する
// 探すたびに並びが変わると、目的のものを見失いやすいため
// -------------------------------------------------------------------------------
void Editor::AssetDatabase::Refresh()
{
	m_Entries.clear();

	std::error_code error;
	if (!std::filesystem::exists(m_CurrentDirectory, error))
	{
		// 表示中のフォルダが外部から消された場合はルートへ戻す
		m_CurrentDirectory = m_RootDirectory;
	}

	for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory, error))
	{
		AssetEntry item;
		item.Path			= entry.path();
		item.Name			= entry.path().filename().string();
		item.IsDirectory	= entry.is_directory(error);
		item.Type			= item.IsDirectory ? AssetType::Folder : ClassifyPath(entry.path());

		m_Entries.push_back(std::move(item));
	}

	std::sort(m_Entries.begin(), m_Entries.end(),
		[](const AssetEntry& _a, const AssetEntry& _b)
		{
			if (_a.IsDirectory != _b.IsDirectory)
			{
				return _a.IsDirectory;	// フォルダを先に
			}
			return ToLower(_a.Name) < ToLower(_b.Name);
		});

	RebuildBreadcrumb();
}

// -------------------------------------------------------------------------------
// 外部での変更を取り込む
//
// エクスプローラーでファイルを足したり消したりしても、
// 次のフレームには一覧へ反映される
// -------------------------------------------------------------------------------
bool Editor::AssetDatabase::Update()
{
	if (!m_Watcher.Poll())
	{
		return false;
	}

	Refresh();
	return true;
}

// -------------------------------------------------------------------------------
// 表示中フォルダの移動
// -------------------------------------------------------------------------------
void Editor::AssetDatabase::SetCurrentDirectory(const std::filesystem::path& _path)
{
	std::error_code error;
	if (!std::filesystem::is_directory(_path, error))
	{ return; }

	m_CurrentDirectory = _path;
	Refresh();
}

void Editor::AssetDatabase::GoUp()
{
	// ルートより上へは行かせない。コンテンツの外へ迷い込まないようにするため
	if (m_CurrentDirectory == m_RootDirectory)
	{ return; }

	SetCurrentDirectory(m_CurrentDirectory.parent_path());
}

// -------------------------------------------------------------------------------
// 作成
// -------------------------------------------------------------------------------
bool Editor::AssetDatabase::CreateFolder(std::string_view _baseName, std::filesystem::path& _outPath)
{
	const std::filesystem::path path = MakeUniquePath(m_CurrentDirectory, _baseName, "");

	std::error_code error;
	if (!std::filesystem::create_directory(path, error) || error)
	{
		ELOG("AssetDatabase::CreateFolder() failed");
		return false;
	}

	_outPath = path;
	Refresh();
	return true;
}

bool Editor::AssetDatabase::CreateFile(
	std::string_view		_baseName,
	std::string_view		_extension,
	std::string_view		_contents,
	std::filesystem::path&	_outPath)
{
	const std::filesystem::path path = MakeUniquePath(m_CurrentDirectory, _baseName, _extension);

	std::ofstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		ELOG("AssetDatabase::CreateFile() failed to open file");
		return false;
	}

	file.write(_contents.data(), static_cast<std::streamsize>(_contents.size()));
	file.close();

	_outPath = path;
	Refresh();
	return true;
}

// -------------------------------------------------------------------------------
// 削除
//
// フォルダは中身ごと消えるため、呼び出し側で確認を取ってから使う想定
// -------------------------------------------------------------------------------
bool Editor::AssetDatabase::Delete(const std::filesystem::path& _path)
{
	std::error_code error;

	// ルート自体は消させない
	if (_path == m_RootDirectory)
	{ return false; }

	const bool removed = std::filesystem::remove_all(_path, error) > 0;
	if (error)
	{
		ELOG("AssetDatabase::Delete() failed");
		return false;
	}

	// 表示中フォルダを消した場合は親へ戻る
	if (m_CurrentDirectory == _path)
	{
		m_CurrentDirectory = _path.parent_path();
	}

	Refresh();
	return removed;
}

bool Editor::AssetDatabase::Rename(const std::filesystem::path& _path, std::string_view _newName)
{
	if (_newName.empty() || _path == m_RootDirectory)
	{ return false; }

	const std::filesystem::path destination = _path.parent_path() / std::filesystem::path(std::string(_newName));

	std::error_code error;
	if (std::filesystem::exists(destination, error))
	{
		return false;	// 同名がある場合は何もしない
	}

	std::filesystem::rename(_path, destination, error);
	if (error)
	{
		ELOG("AssetDatabase::Rename() failed");
		return false;
	}

	Refresh();
	return true;
}

// -------------------------------------------------------------------------------
// 拡張子から種類を判定する
// -------------------------------------------------------------------------------
Editor::AssetType Editor::AssetDatabase::ClassifyPath(const std::filesystem::path& _path)
{
	const std::string extension = ToLower(_path.extension().string());

	if (extension == ".effect")							{ return AssetType::Effect;  }
	if (extension == ".scene")							{ return AssetType::Scene;   }
	if (extension == ".fbx" || extension == ".obj" ||
		extension == ".gltf" || extension == ".glb")	{ return AssetType::Model;   }
	if (extension == ".png" || extension == ".jpg" ||
		extension == ".jpeg" || extension == ".dds" ||
		extension == ".tga")							{ return AssetType::Texture; }
	if (extension == ".hlsl" || extension == ".slang")	{ return AssetType::Shader;  }
	if (extension == ".txt" || extension == ".json" ||
		extension == ".md")								{ return AssetType::Text;    }

	return AssetType::Unknown;
}

std::string_view Editor::AssetDatabase::GetTypeLabel(AssetType _type)
{
	switch (_type)
	{
	case AssetType::Folder:		return "FOLDER";
	case AssetType::Effect:		return "EFFECT";
	case AssetType::Scene:		return "SCENE";
	case AssetType::Model:		return "MODEL";
	case AssetType::Texture:	return "TEX";
	case AssetType::Shader:		return "SHADER";
	case AssetType::Text:		return "TEXT";
	default:					return "FILE";
	}
}

// -------------------------------------------------------------------------------
// 名前が重複しないパスを作る
//
// "NewFolder" が既にあれば "NewFolder_1"、それもあれば "NewFolder_2" と続ける
// -------------------------------------------------------------------------------
std::filesystem::path Editor::AssetDatabase::MakeUniquePath(
	const std::filesystem::path&	_directory,
	std::string_view				_baseName,
	std::string_view				_extension) const
{
	const std::string base		= std::string(_baseName);
	const std::string extension	= std::string(_extension);

	std::filesystem::path candidate = _directory / (base + extension);

	std::error_code error;
	for (int suffix = 1; std::filesystem::exists(candidate, error); ++suffix)
	{
		candidate = _directory / (base + "_" + std::to_string(suffix) + extension);
	}

	return candidate;
}

// -------------------------------------------------------------------------------
// ルートから現在位置までの並びを作る
// パンくずリストを描くために使う
// -------------------------------------------------------------------------------
void Editor::AssetDatabase::RebuildBreadcrumb()
{
	m_Breadcrumb.clear();

	// 現在位置からルートへさかのぼり、あとで反転させる
	std::filesystem::path current = m_CurrentDirectory;

	while (true)
	{
		m_Breadcrumb.push_back(current);

		if (current == m_RootDirectory || !current.has_parent_path() || current.parent_path() == current)
		{
			break;
		}

		current = current.parent_path();
	}

	std::reverse(m_Breadcrumb.begin(), m_Breadcrumb.end());
}
