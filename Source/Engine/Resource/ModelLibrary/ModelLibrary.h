#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/Mesh/Mesh/Mesh.h>
#include <Engine/Mesh/Material/Material.h>

namespace RHI { class Device; }

// -------------------------------------------------------------------------------
// ModelPart struct
//
// 概要 :
//	モデルの中の「1つ分の描画単位」
//	1つのメッシュと、それに割り当てられたマテリアルの組
//
//	モデルファイル(.fbxなど)は、たいてい複数のメッシュを含む
//	（体・髪・服 が別メッシュになっている、など）
//	MeshComponentが描くのはそのうちの1つなので、
//	「どのモデルの、何番目か」で指せるようにこの単位で並べておく
// -------------------------------------------------------------------------------
struct ModelPart
{
	Mesh*		pMesh		= nullptr;	// 所有権はModelResourceが持つ
	Material*	pMaterial	= nullptr;	// マテリアルが無いメッシュではnullptr
	std::string	Name;					// 表示用（"Part 0" など）
};

// -------------------------------------------------------------------------------
// ModelResource class
//
// 概要 :
//	読み込み済みのモデルファイル1つ分
//
//	GPUリソース(Mesh / Material)の所有者であり、
//	使う側へは常に参照(ModelPart)だけを渡す
//	同じモデルを何体置いても、GPU上の実体は1つで済む
// -------------------------------------------------------------------------------
class ModelResource
{
public:

	const std::filesystem::path& GetPath() const	{ return m_Path; }

	// プロジェクトからの相対パス。プレファブやシーンへ保存するのはこちら
	const std::string& GetKey() const				{ return m_Key; }

	size_t GetPartCount() const						{ return m_Parts.size(); }
	const std::vector<ModelPart>& GetParts() const	{ return m_Parts; }

	// -------------------------------------------------------------------------------
	// @brief	指定番号のパートを返す
	//
	// @return	範囲外の場合はnullptr
	// -------------------------------------------------------------------------------
	const ModelPart* GetPart(size_t _index) const
	{
		return (_index < m_Parts.size()) ? &m_Parts[_index] : nullptr;
	}

private:

	// 生成と中身の構築はModelLibraryだけが行う
	friend class ModelLibrary;

	std::filesystem::path					m_Path;			// 実際に読んだ絶対パス
	std::string								m_Key;			// プロジェクトからの相対パス
	std::vector<std::unique_ptr<Mesh>>		m_Meshes;
	std::vector<std::unique_ptr<Material>>	m_Materials;
	std::vector<ModelPart>					m_Parts;
};

// -------------------------------------------------------------------------------
// ModelLibrary class
//
// 概要 :
//	モデルファイルを読み込み、GPUリソースとして持ち続ける置き場
//
//	なぜ必要か :
//		以前はシーンがモデルを読み、Mesh / Material を自分で抱えていた
//		そのためエディタからモデルを割り当てようとしても、
//		「誰がそのGPUリソースを持つのか」が決まらなかった
//		また同じモデルを2体置くと、GPU上に2つ読み込まれてしまう
//
//		読み込みと所有をここへ集めることで
//			・エディタからでもシーンからでも、同じ手順でモデルを指せる
//			・同じファイルは1回しか読まれない
//			・シーンを切り替えても読み直しが起きない
//		という状態になる
//
//	寿命 :
//		シーンより長生きする必要があるため、所有者はApplication
//		シーンやエディタは参照だけを受け取る
//
//	パスの扱い :
//		外へ見せる名前(Key)はプロジェクトからの相対パスにそろえる
//		プレファブやシーンのファイルへ絶対パスを書くと、
//		別の場所へプロジェクトを移した時点で開けなくなるため
// -------------------------------------------------------------------------------
class ModelLibrary
{
public:

	ModelLibrary()	= default;
	~ModelLibrary()	{ Term(); }

	// -------------------------------------------------------------------------------
	// @brief	初期化
	//
	// @param[in]	_pDevice		GPUリソースの生成に使うデバイス
	// @param[in]	_projectRoot	相対パスの基準にするフォルダ
	// -------------------------------------------------------------------------------
	bool Init(RHI::Device* _pDevice, const std::filesystem::path& _projectRoot);

	// @brief	読み込み済みのモデルをすべて解放する
	void Term();

	// -------------------------------------------------------------------------------
	// @brief	モデルを読み込む（すでに読み込み済みならそれを返す）
	//
	//	絶対パス・相対パスのどちらでも受け付ける
	//
	// @param[in]	_path	モデルファイルのパス
	// @return	読み込んだモデル。失敗した場合はnullptr
	// -------------------------------------------------------------------------------
	const ModelResource* Load(const std::filesystem::path& _path);

	// @brief	読み込み済みのモデルを探す（読み込みは行わない）
	const ModelResource* Find(const std::filesystem::path& _path) const;

	// @brief	読み込み済みモデルの相対パス一覧（エディタの選択肢に使う）
	std::vector<std::string> GetLoadedKeys() const;

	// -------------------------------------------------------------------------------
	// パスの変換
	//
	//	Keyはプロジェクトからの相対パスを "/" 区切り・小文字化せずに保った文字列
	//	同じファイルを別の書き方で指しても同じKeyになるようにする
	// -------------------------------------------------------------------------------
	std::string				ToKey(const std::filesystem::path& _path) const;
	std::filesystem::path	ToAbsolute(const std::filesystem::path& _path) const;

	const std::filesystem::path& GetProjectRoot() const { return m_ProjectRoot; }

private:

	RHI::Device*			m_pDevice = nullptr;	// 所有権なし
	std::filesystem::path	m_ProjectRoot;

	// Key(相対パス) → 読み込み済みモデル
	// unique_ptrで持つのは、要素を足しても既存への参照が無効にならないようにするため
	std::unordered_map<std::string, std::unique_ptr<ModelResource>> m_Models;

	ModelLibrary	(const ModelLibrary&) = delete;
	void operator = (const ModelLibrary&) = delete;
};
