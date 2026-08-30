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
		// std::transformを使って文字列内の各文字を順番に小文字へ変換する
		// 
		// std::tolowerにcharをそのまま渡すと、
		// charが符号付きの場合に負数となる可能性があるため、一度unsigned charとして受け取る
		std::transform(_value.begin(), _value.end(), _value.begin(),
			[](unsigned char _ch) { return static_cast<char>(std::tolower(_ch)); });
		return _value;
	}
}

// -------------------------------------------------------------------------------
// 初期化
// 
//	AssetDatabaseが管理するルートフォルダを設定し、
//	ファイル監視と最初のアセット一覧生成を行う
// -------------------------------------------------------------------------------
bool Editor::AssetDatabase::Init(const std::filesystem::path& _rootPath)
{
	// std::filesystemの関数にerror_codeを渡すことで、
	// ファイルシステムエラーを例外ではなく戻り値として処理する
	// 
	// Editorのファイル操作中に例外でアプリ全体が停止するのを避けるため
	std::error_code error;

	// -------------------------------------------------------------------------------
	// アセットルートの存在確認
	// -------------------------------------------------------------------------------

	// ルートが無ければ作る。初回起動でもコンテンツブラウザが空で開けるようにする
	if (!std::filesystem::exists(_rootPath, error))
	{
		// create_directioriesを使用することで、
		// 途中のディレクトリがなくてもまとめて作成できる
		std::filesystem::create_directories(_rootPath, error);
		// フォルダ作成に失敗した場合、
		// AssetDatabaseの基準となる場所自体が存在しないため初期化失敗
		if (error)
		{
			ELOG("AssetDatabase::Init() failed to create root directory");
			return false;
		}
	}

	// -------------------------------------------------------------------------------
	// ルートディレクトリを絶対パスとして保持
	// -------------------------------------------------------------------------------

	// 相対パスのまま保持すると、Current Working Directoryの変化によって
	// 同じパスが別の場所を指す可能性がある
	// 
	// そのためAssetDatabase内では絶対パスへ統一して使う
	m_RootDirectory		= std::filesystem::absolute(_rootPath, error);

	// 初期状態ではルートディレクトリを表示する
	m_CurrentDirectory	= m_RootDirectory;

	// ルート以下をまとめて(再帰的に)監視する
	// これにより、エディター外部のExplorerなどからファイルを削除などしても検知できる
	// 失敗しても手動の再読み込みは使えるため、致命的な扱いにはしない
	if (!m_Watcher.Start(m_RootDirectory, true))
	{
		// ファイル監視に失敗してもRefresh()を手動で呼べば一覧自体は更新できる
		// そのためAssetDatabase全体の初期化失敗とはせず、「自動更新だけ使えない状態」として実行する
		ELOG("AssetDatabase::Init() failed to watch content directory (auto refresh disabled)");
	}

	// 最初のアセット一覧を構築する
	Refresh();
	return true;
}

// -------------------------------------------------------------------------------
// 現在表示しているフォルダの内容を再取得
// 
//	m_CurrentDirectoryの中に存在するファイル・フォルダを走査し、
//	Content Browser で表示するためのm_Entriesを作り直す
// 
//	並び順は
//	1. フォルダ
//	2. ファイル
//	とし、それぞれ名前順に並べる
// -------------------------------------------------------------------------------
void Editor::AssetDatabase::Refresh()
{
	// -------------------------------------------------------------------------------
	// 前回取得した一覧を破棄する
	// 
	// 古い情報を残さないため。
	// -------------------------------------------------------------------------------
	m_Entries.clear();

	// Explorerなど外部から現在表示されているフォルダ自体が削除されると、
	// m_CurrentDirectoryが存在しないパスを指したままになる
	std::error_code error;
	if (!std::filesystem::exists(m_CurrentDirectory, error))
	{
		// 表示中のフォルダが外部から消された場合はルートへ戻す
		m_CurrentDirectory = m_RootDirectory;
	}

	// -------------------------------------------------------------------------------
	// 現在のフォルダを走査
	// -------------------------------------------------------------------------------

	// directory_iteratorは指定したディレクトリ直下だけを列挙する
	// 
	// recursive_directory_iteratorではないので、子フォルダ内部のファイルまではここでは表示しない
	// Content Browserとして、「現在開いているフォルダの中身だけ」を表示するため
	for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory, error))
	{
		// ファイルまたはフォルダ1件分の表示情報を構築する
		AssetEntry item;
		item.Path			= entry.path();						// アセットの完全なパスを保持する
		item.Name			= entry.path().filename().string();	// ContentBrowser上で表示する名前
		item.IsDirectory	= entry.is_directory(error);		// この項目がフォルダかどうかを判定する
		// フォルダの場合は必ずFolder
		// ファイルの場合は拡張子を調べてTexture/Model/Shaderなどへ分類する
		item.Type			= item.IsDirectory ? AssetType::Folder : ClassifyPath(entry.path());

		// 完成した項目を一覧へ追加する
		m_Entries.push_back(std::move(item));
	}

	// -------------------------------------------------------------------------------
	// 表示順を整理
	// -------------------------------------------------------------------------------
	std::sort(m_Entries.begin(), m_Entries.end(),
		[](const AssetEntry& _a, const AssetEntry& _b)
		{
			// 一方だけがフォルダの場合はフォルダをファイルより先に並べる
			// _aがフォルダならtrueが返るため、_aが_bより前に配置される
			if (_a.IsDirectory != _b.IsDirectory)
			{
				return _a.IsDirectory;	// フォルダを先に
			}
			// 同じ種類同士なら名前順に並べる
			// 文字列をすべて小文字に変換っして判定
			return ToLower(_a.Name) < ToLower(_b.Name);
		});

	// 現在位置が変わっている可能性があるため、ContentBrowser上部に表示するパンくずリストも作り直す
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
	// Poll()がfalseの場合は、前回からファイルシステムに変更がない
	if (!m_Watcher.Poll())
	{
		return false;
	}

	// ファイルの追加・削除・名前変更などが発生したため、現在表示中のディレクトリ情報を再取得
	Refresh();
	return true;
}

// -------------------------------------------------------------------------------
// 表示中フォルダの移動
// -------------------------------------------------------------------------------
void Editor::AssetDatabase::SetCurrentDirectory(const std::filesystem::path& _path)
{
	std::error_code error;

	// 移動先が実際に存在するディレクトリでなければ何もしない
	// 
	// ファイルパスを誤って渡した場合や、外部からフォルダを削除された場合への安全策
	if (!std::filesystem::is_directory(_path, error))
	{ return; }

	// 現在表示するフォルダを変更
	m_CurrentDirectory = _path;
	// 新しいフォルダの内容をすぐ一覧へ反映する
	Refresh();
}

// -------------------------------------------------------------------------------
// 1階層上のフォルダへ移動
// -------------------------------------------------------------------------------
void Editor::AssetDatabase::GoUp()
{
	// ルートより上へは行かせない。コンテンツの外へ迷い込まないようにするため
	if (m_CurrentDirectory == m_RootDirectory)
	{ return; }

	// 現在位置の親ディレクトリへ移動する
	SetCurrentDirectory(m_CurrentDirectory.parent_path());
}

// -------------------------------------------------------------------------------
// フォルダ作成
// -------------------------------------------------------------------------------
bool Editor::AssetDatabase::CreateFolder(std::string_view _baseName, std::filesystem::path& _outPath)
{
	// -------------------------------------------------------------------------------
	// 重複しないフォルダ名を決定
	// -------------------------------------------------------------------------------

	// 例えばNewFolderを要求した場合
	// NewFolder, NewFolder_1, NewFolder_2,...のように既存名と重複しないようにパスを作成する
	// フォルダなので拡張子はから文字列を指定
	const std::filesystem::path path = MakeUniquePath(m_CurrentDirectory, _baseName, "");

	std::error_code error;

	// 実際にOS上でディレクトリを作成する
	if (!std::filesystem::create_directory(path, error) || error)
	{
		ELOG("AssetDatabase::CreateFolder() failed");
		return false;
	}

	// 呼び出し側に、実際に作られたパスを返す
	_outPath = path;
	// 新しいフォルダをコンテンツブラウザーに即座に反映
	Refresh();
	return true;
}

// -------------------------------------------------------------------------------
// ファイル作成
// -------------------------------------------------------------------------------
bool Editor::AssetDatabase::CreateFile(
	std::string_view		_baseName,
	std::string_view		_extension,
	std::string_view		_contents,
	std::filesystem::path&	_outPath)
{
	// 重複しないファイルパスを作成
	const std::filesystem::path path = MakeUniquePath(m_CurrentDirectory, _baseName, _extension);

	// -------------------------------------------------------------------------------
	// ファイルを開く
	// -------------------------------------------------------------------------------

	// binaryを指定することで改行コードなどをiostream側で変換せず、
	// _contentsの内容をそのままファイルへ書き込む
	std::ofstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		ELOG("AssetDatabase::CreateFile() failed to open file");
		return false;
	}

	// string_viewが保持するバイト列をそのままファイルへ書き込む
	file.write(_contents.data(), static_cast<std::streamsize>(_contents.size()));
	// ファイルを明示的に閉じる
	// 一応デストラクタで自動的に閉じられる
	file.close();

	// 実際に作成されたファイルパスを呼び出し側に返す
	_outPath = path;
	// コンテンツブラウザーの一覧に新しいファイルを反映
	Refresh();
	return true;
}

// -------------------------------------------------------------------------------
// ファイル、フォルダ削除
//
// フォルダは中身ごと消えるため、呼び出し側で確認を取ってから使う想定
// -------------------------------------------------------------------------------
bool Editor::AssetDatabase::Delete(const std::filesystem::path& _path)
{
	std::error_code error;

	// ルート自体は消させない
	if (_path == m_RootDirectory)
	{ return false; }

	// -------------------------------------------------------------------------------
	// ファイル、フォルダを削除
	// -------------------------------------------------------------------------------

	// remove_allを使うことで、ファイルだけやフォルダからその中身のファイルまで再帰的に削除できる
	// 戻り値は削除されたファイルの数
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

	// 削除結果をコンテンツブラウザーに反映
	Refresh();
	return removed;
}

// -------------------------------------------------------------------------------
// ファイル、フォルダの名前変更
// -------------------------------------------------------------------------------
bool Editor::AssetDatabase::Rename(const std::filesystem::path& _path, std::string_view _newName)
{
	// -------------------------------------------------------------------------------
	// 不正なRename要求を除外
	// -------------------------------------------------------------------------------
	if (_newName.empty() || _path == m_RootDirectory)
	{ return false; }

	// -------------------------------------------------------------------------------
	// 新しいパスを生成
	// -------------------------------------------------------------------------------

	// 名前変更はフォルダorファイルの最後の名前だけ_newNameに置き換える
	const std::filesystem::path destination = _path.parent_path() / std::filesystem::path(std::string(_newName));

	std::error_code error;

	// -------------------------------------------------------------------------------
	// 同名ファイル、フォルダが存在しないか確認
	// -------------------------------------------------------------------------------

	// 同名がある場合は何もしない
	if (std::filesystem::exists(destination, error))
	{
		return false;
	}

	// OS上で実際にファイル、フォルダの名前を変更する
	std::filesystem::rename(_path, destination, error);
	if (error)
	{
		ELOG("AssetDatabase::Rename() failed");
		return false;
	}

	// 変更後の名前をコンテンツブラウザーに反映
	Refresh();
	return true;
}

// -------------------------------------------------------------------------------
// 拡張子から種類を判定する
// -------------------------------------------------------------------------------
Editor::AssetType Editor::AssetDatabase::ClassifyPath(const std::filesystem::path& _path)
{
	// .PNGや.pngについても同じ種類として判定できるよう、小文字へ統一してから比較する
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

	// 上記以外のファイルはUnknownとして扱う
	return AssetType::Unknown;
}

// -------------------------------------------------------------------------------
// AssetTypeをContentBrowser表示用文字列へ変換
// 内部ではenumで管理するが、UI上では文字列として使用するために使用
// -------------------------------------------------------------------------------
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
	// string_viewは文字列そのものを所有していないため、
	// この関数内ではstd::stringへ変換しておく
	const std::string base		= std::string(_baseName);
	const std::string extension	= std::string(_extension);

	// -------------------------------------------------------------------------------
	// まず番号なしの名前を候補にする
	// -------------------------------------------------------------------------------
	
	// 例えば
	// _directory = Content/Shader/
	// _baseName  = NewShader
	// _extension = .slang
	// の場合、Content/Shader/NewShader.slangが最初の候補になる
	std::filesystem::path candidate = _directory / (base + extension);

	std::error_code error;

	// -------------------------------------------------------------------------------
	// 同名が存在する間、末尾番号を増やす
	// -------------------------------------------------------------------------------

	// candidateが既に存在している間だけループする
	// suffixは1から開始するため、
	// NewShader.slang , NewShader_1.slang , NewShader_2.slang という名前になる
	for (int suffix = 1; std::filesystem::exists(candidate, error); ++suffix)
	{
		candidate = _directory / (base + "_" + std::to_string(suffix) + extension);
	}

	// ファイルシステム上にまだ存在していないパスを返す
	// 
	// この関数自身ではファイルやフォルダは作成しない
	return candidate;
}

// -------------------------------------------------------------------------------
// ルートから現在位置までの並びを作る
// パンくずリストを描くために使う
// -------------------------------------------------------------------------------
void Editor::AssetDatabase::RebuildBreadcrumb()
{
	// 前回作成したパンくず情報を破棄する
	m_Breadcrumb.clear();

	// -------------------------------------------------------------------------------
	// 現在位置からルートへ向かって逆方向にたどる
	// -------------------------------------------------------------------------------
	
	// 一番根っこから開始する
	std::filesystem::path current = m_CurrentDirectory;

	while (true)
	{
		// 現在の階層を記録する
		// 最初は Content/Texture/Characterにあたる部分が追加される
		m_Breadcrumb.push_back(current);

		// -------------------------------------------------------------------------------
		// これ以上親に進んではいけない条件
		// -------------------------------------------------------------------------------
		
		// 1. AssetDatabaseのルートへ到達した
		// 2. 親パスそのものが存在しない
		// 3. parent_path()が自分自身を返す→ファイルシステム上の最上位へ到達している
		// のどれかで探索を終了
		if (current == m_RootDirectory || !current.has_parent_path() || current.parent_path() == current)
		{
			break;
		}

		// 1つ上の階層へ移動する
		current = current.parent_path();
	}
	// -------------------------------------------------------------------------------
	// 順番を反転
	// -------------------------------------------------------------------------------

	// ここまでは現在位置から逆順に追加しているため
	// Character > Texture > Content
	// という順番
	// UIでは Content > Texture > Characterという順番で表示したいため反転する
	std::reverse(m_Breadcrumb.begin(), m_Breadcrumb.end());
}
