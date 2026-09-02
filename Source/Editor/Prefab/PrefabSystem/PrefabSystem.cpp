// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "PrefabSystem.h"

#include <Engine/Asset/PrefabAsset/PrefabAsset.h>
#include <Engine/GameObject/GameObject.h>
#include <Engine/GameObject/GameObjectManager.h>

#include <Editor/Assets/AssetDatabase.h>
#include <Editor/Commands/ObjectCommands/ObjectCommands.h>

// -------------------------------------------------------------------------------
// コンテンツフォルダへ保存する
// -------------------------------------------------------------------------------
bool Editor::PrefabSystem::SaveToContent(
	EditorContext&			_ctx,
	const GameObject&		_object,
	std::filesystem::path&	_outPath)
{
	if (_ctx.pAssets == nullptr)
	{
		return false;	// コンテンツブラウザが無い
	}

	// 設計図への写し取りと文字列化はエンジン側が受け持つ
	const std::string contents = PrefabIO::Serialize(PrefabIO::CaptureFromObject(_object));

	// ファイルの作成そのものはAssetDatabaseの仕事
	// 保存先フォルダの決定も、重複時の連番も、すでにそちらが持っている
	return _ctx.pAssets->CreateFile(
		_object.GetName(),
		PrefabIO::kExtension,
		contents,
		_outPath);
}

// -------------------------------------------------------------------------------
// ファイルを読み、シーンへ1体置く
// -------------------------------------------------------------------------------
GameObject* Editor::PrefabSystem::InstantiateFromFile(
	EditorContext&					_ctx,
	const std::filesystem::path&	_path)
{
	if (_ctx.pObjects == nullptr)
	{
		return nullptr;	// シーンが読み込まれていない
	}

	PrefabAsset prefab;

	if (!PrefabIO::LoadFromFile(_path, prefab))
	{
		return nullptr;
	}

	auto pObject = PrefabIO::Build(*_ctx.pObjects, prefab);

	if (pObject == nullptr)
	{
		return nullptr;
	}

	// 置く操作は、手で作った場合とまったく同じ経路を通す
	// こうしておくと、配置もそのままUndoで取り消せる
	return ObjectCommands::Spawn(_ctx, std::move(pObject), "プレファブの配置");
}
