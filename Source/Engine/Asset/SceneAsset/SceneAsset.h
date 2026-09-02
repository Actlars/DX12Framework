#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/Asset/PrefabAsset/PrefabAsset.h>

class GameObjectManager;

// -------------------------------------------------------------------------------
// SceneAsset struct
//
// 概要 :
//	「シーン1つ分の中身」を、値だけで表したもの
//
//	プレファブが1体分の設計図なら、こちらはシーン全体の設計図にあたる
//	中身は、置いてあるオブジェクトを1体ずつ設計図にしたものの並び
//
//	プレファブと同じ形を使い回す理由 :
//		「オブジェクト1体を値へ写す / 値から組み立てる」という処理は
//		プレファブのためにすでに用意してある
//		シーン用にもう一組作ると、コンポーネントを増やすたびに
//		2か所へ手を入れることになり、片方だけ忘れる原因になる
//
//	保存しないもの :
//		カメラの位置や、シーンが持つ描画設定は今のところ保存しない
//		（それらはまだGameSceneのコードが直接持っているため）
//		保存できるのは「GameObjectManagerに入っているもの」だけ
// -------------------------------------------------------------------------------
struct SceneAsset
{
	std::string	Name = "New Scene";

	// 置いてある順に並ぶ。読み込みもこの順で行う
	std::vector<PrefabAsset> Objects;
};

// -------------------------------------------------------------------------------
// シーンの読み書きと、GameObjectManagerとの相互変換
// -------------------------------------------------------------------------------
namespace SceneIO
{
	// シーンファイルの拡張子
	constexpr std::string_view kExtension = ".scene";

	// テキスト(JSON)との相互変換
	std::string Serialize(const SceneAsset& _scene);
	bool		Deserialize(std::string_view _json, SceneAsset& _outScene);

	// ファイルとの読み書き
	bool SaveToFile(const std::filesystem::path& _path, const SceneAsset& _scene);
	bool LoadFromFile(const std::filesystem::path& _path, SceneAsset& _outScene);

	// -------------------------------------------------------------------------------
	// @brief	今シーンに置いてあるものを、まるごと写し取る
	//
	// @param[in]	_objects	写し取る相手
	// @param[in]	_name		シーンに付ける名前
	// -------------------------------------------------------------------------------
	SceneAsset Capture(const GameObjectManager& _objects, std::string_view _name);

	// -------------------------------------------------------------------------------
	// @brief	シーンの中身を置き換える
	//
	//	今あるオブジェクトをすべて消してから、設計図どおりに置き直す
	//	「開く」操作にあたるため、元の中身は残らない
	//
	//	注意 :
	//		Update / Submit のループ中に呼んではいけない
	//		（その場でオブジェクトの一覧を作り直すため）
	//		エディタはUIの組み立て中、つまりループの外から呼んでいる
	//
	// @param[in,out]	_objects	置き換える相手
	// @param[in]		_scene		もとにする設計図
	// @return	置いたオブジェクトの数
	// -------------------------------------------------------------------------------
	size_t Apply(GameObjectManager& _objects, const SceneAsset& _scene);

	// @brief	そのパスがシーンファイルかどうか
	bool IsScenePath(const std::filesystem::path& _path);
}
