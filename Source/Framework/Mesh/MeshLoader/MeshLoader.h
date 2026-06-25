#pragma once

// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "../ResData.h"

// -------------------------------------------------------------------------------
// MeshLoader class
// 
// 概要 : 
//	Assimpを使ってファイルからメッシュとマテリアルの生データを読み込む
//	出力は ResMesh / ResMaterial の配列で、GPUリソースは一切生成しない
//	全メソッドがstaticのユーティリティクラス
//	対応フォーマット : Assimpが対応する全形式
// 
// 使い方 : 
//	std::vector<ResMesh>		meshes;
//	std::vector<ResMaterial>	materials;
// if(!MeshLoader::Load(L"Assets/model.fbx", meshes, materials))
// { /* エラー処理 */ }
// -------------------------------------------------------------------------------
class MeshLoader
{
public:

	// -------------------------------------------------------------------------------
	// @brief	メッシュファイルをロードする
	// 
	// @param[in]	_path		ファイルパス
	// @param[in]	_meshes		ロードしたメッシュの格納先
	// @param[out]	_materials	ロードしたマテリアルの格納先
	// @retval	true	成功
	// @retval	false	失敗（ファイルなし・フォーマット不正など）
	// -------------------------------------------------------------------------------
	static bool Load(
		const std::wstring&			_path,
		std::vector<ResMesh>&		_meshes,
		std::vector<ResMaterial>&	_materials);

private:

	MeshLoader()	= delete;
	~MeshLoader()	= delete;

};


