#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Editor/Assets/DirectoryWatcher/DirectoryWatcher.h>

namespace Editor
{
	// -------------------------------------------------------------------------------
	// AssetType : enum
	//
	// 概要 :
	//	コンテンツブラウザがアイコンの色と「開いたときの動作」を決めるための種類
	//	判定は拡張子だけで行う
	// -------------------------------------------------------------------------------
	enum class AssetType : uint8_t
	{
		Folder,
		Effect,		// .effect  Niagara風のエフェクトエディタで開く
		Scene,		// .scene
		Model,		// .fbx / .obj / .gltf
		Texture,	// .png / .jpg / .dds
		Shader,		// .hlsl / .slang
		Text,		// .txt / .json / .md
		Unknown,
	};

	// -------------------------------------------------------------------------------
	// AssetEntry struct
	//
	// 概要 :
	//	コンテンツブラウザに1つ並ぶ項目
	//	ディスク上の実体1つ（ファイルまたはフォルダ）に対応する
	// -------------------------------------------------------------------------------
	struct AssetEntry
	{
		std::filesystem::path	Path;			// 絶対パス
		std::string				Name;			// 表示名(拡張子を含むファイル名)
		AssetType				Type = AssetType::Unknown;
		bool					IsDirectory = false;
	};

	// -------------------------------------------------------------------------------
	// AssetDatabase class
	//
	// 概要 :
	//	コンテンツブラウザが表示する「ファイルの木」を担当するクラス
	//
	//	UIを一切持たず、ディスク上のフォルダ構成を読み書きすることだけに専念する
	//	これにより、コンテンツブラウザのパネルは「どう見せるか」だけを書けばよくなる
	//
	//	一覧はキャッシュして持つ
	//	毎フレームディスクを走査すると重いため、変更があったときだけRefreshする
	// -------------------------------------------------------------------------------
	class AssetDatabase
	{
	public:

		// -------------------------------------------------------------------------------
		// @brief	ルートフォルダを決めて初期化する
		//
		//	フォルダが存在しない場合はここで作成する
		//
		// @param[in]	_rootPath	コンテンツの置き場所
		// -------------------------------------------------------------------------------
		bool Init(const std::filesystem::path& _rootPath);

		// @brief	現在のフォルダの中身を読み直す
		void Refresh();

		// -------------------------------------------------------------------------------
		// @brief	外部（エクスプローラー等）での変更を取り込む
		//
		//	毎フレームの先頭で呼ぶ
		//	変化がなければ何もしないため、コストはほぼゼロ
		//
		// @return	true : 読み直しが発生した（表示中の一覧が入れ替わっている）
		// -------------------------------------------------------------------------------
		bool Update();

		// -------------------------------------------------------------------------------
		// 表示中フォルダの移動
		// -------------------------------------------------------------------------------
		void SetCurrentDirectory(const std::filesystem::path& _path);
		const std::filesystem::path& GetCurrentDirectory() const { return m_CurrentDirectory; }
		const std::filesystem::path& GetRootDirectory()	  const { return m_RootDirectory; }

		// @brief	1つ上のフォルダへ移動する。ルートより上へは行かない
		void GoUp();

		// @brief	ルートから現在位置までの、パンくずリスト用の並び
		const std::vector<std::filesystem::path>& GetBreadcrumb() const { return m_Breadcrumb; }

		// @brief	現在のフォルダの中身。フォルダが先、ファイルが後の順に並ぶ
		const std::vector<AssetEntry>& GetEntries() const { return m_Entries; }

		// -------------------------------------------------------------------------------
		// 作成・削除・リネーム
		//
		//	いずれも成功したらキャッシュを更新する
		//	失敗時はfalseを返すだけで、例外は投げない
		// -------------------------------------------------------------------------------

		// @brief	現在のフォルダに新しいフォルダを作る。名前が重複したら連番を付ける
		bool CreateFolder(std::string_view _baseName, std::filesystem::path& _outPath);

		// -------------------------------------------------------------------------------
		// @brief	現在のフォルダに新しいファイルを作る
		//
		// @param[in]	_baseName	拡張子を含まない名前。重複したら連番を付ける
		// @param[in]	_extension	". "を含む拡張子
		// @param[in]	_contents	初期内容
		// -------------------------------------------------------------------------------
		bool CreateFile(
			std::string_view			_baseName,
			std::string_view			_extension,
			std::string_view			_contents,
			std::filesystem::path&		_outPath);

		bool Delete(const std::filesystem::path& _path);
		bool Rename(const std::filesystem::path& _path, std::string_view _newName);

		// -------------------------------------------------------------------------------
		// @brief	拡張子から種類を判定する
		// -------------------------------------------------------------------------------
		static AssetType ClassifyPath(const std::filesystem::path& _path);

		// @brief	種類に対応する表示用の短いラベル("FOLDER"等)
		static std::string_view GetTypeLabel(AssetType _type);

	private:

		// 名前が重複しないよう、必要なら "_1", "_2" と連番を足す
		std::filesystem::path MakeUniquePath(
			const std::filesystem::path&	_directory,
			std::string_view				_baseName,
			std::string_view				_extension) const;

		void RebuildBreadcrumb();

		std::filesystem::path				m_RootDirectory;
		std::filesystem::path				m_CurrentDirectory;
		std::vector<AssetEntry>				m_Entries;
		std::vector<std::filesystem::path>	m_Breadcrumb;

		// コンテンツフォルダ全体を監視し、外部の変更に自動で追従する
		DirectoryWatcher m_Watcher;
	};
}
