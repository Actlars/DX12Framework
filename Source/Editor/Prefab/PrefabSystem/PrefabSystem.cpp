// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "PrefabSystem.h"

#include <Engine/GameObject/GameObject.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

#include <Editor/Assets/AssetDatabase.h>
#include <Editor/Commands/ObjectCommands/ObjectCommands.h>
#include <Editor/Components/ComponentRegistry/ComponentRegistry.h>
#include <Editor/Scene/GameObjectFactory/GameObjectFactory.h>

namespace
{
	// ファイル全体を文字列として読む
	// プレファブは人が読める小さなテキストなので、一括で読み込んで構わない
	bool ReadTextFile(const std::filesystem::path& _path, std::string& _outText)
	{
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
}

// -------------------------------------------------------------------------------
// シーン上のオブジェクトから設計図を作る
// -------------------------------------------------------------------------------
Editor::PrefabAsset Editor::PrefabSystem::CaptureFromObject(const GameObject& _object)
{
	PrefabAsset prefab;

	prefab.Name		= _object.GetName();
	prefab.Active	= _object.IsActive();

	// -------------------------------------------------------------------------------
	// 登録済みのコンポーネントを、表の順に写し取る
	//
	// 写し取り方はComponentRegistryが持つため、
	// 複製・コンポーネント削除の取り消しと必ず同じ結果になる
	// -------------------------------------------------------------------------------
	for (const ComponentTypeInfo& info : ComponentRegistry::GetAll())
	{
		if (info.Has == nullptr || !info.Has(_object))
		{
			continue;
		}

		PrefabComponentData component;
		component.TypeName	= std::string(info.TypeName);
		component.Values	= ComponentRegistry::CaptureComponent(_object, info);

		prefab.Components.emplace_back(std::move(component));
	}

	return prefab;
}

// -------------------------------------------------------------------------------
// 設計図からオブジェクトを組み立てる
// -------------------------------------------------------------------------------
std::unique_ptr<GameObject> Editor::PrefabSystem::Build(
	const EditorContext&	_ctx,
	const PrefabAsset&		_prefab)
{
	// 名前が衝突しないよう、シーンの状況を見て連番を足す
	auto pObject = std::make_unique<GameObject>(
		GameObjectFactory::MakeUniqueName(_ctx, _prefab.Name));

	pObject->SetActive(_prefab.Active);

	for (const PrefabComponentData& component : _prefab.Components)
	{
		const ComponentTypeInfo* pInfo = ComponentRegistry::Find(component.TypeName);

		if (pInfo == nullptr)
		{
			// 知らない型は読み飛ばす
			// 保存したときより登録が減っていても、残りは復元できるようにするため
			DLOG("PrefabSystem::Build() unknown component type : %s", component.TypeName.c_str());
			continue;
		}

		ComponentRegistry::RestoreComponent(*pObject, *pInfo, component.Values);
	}

	return pObject;
}

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

	const PrefabAsset prefab	= CaptureFromObject(_object);
	const std::string contents	= SerializePrefab(prefab);

	// ファイルの作成そのものはAssetDatabaseの仕事
	// 保存先フォルダの決定も、重複時の連番も、すでにそちらが持っている
	return _ctx.pAssets->CreateFile(
		_object.GetName(),
		kExtension,
		contents,
		_outPath);
}

// -------------------------------------------------------------------------------
// ファイルから読み込む
// -------------------------------------------------------------------------------
bool Editor::PrefabSystem::LoadFromFile(
	const std::filesystem::path&	_path,
	PrefabAsset&					_outPrefab)
{
	std::string text;

	if (!ReadTextFile(_path, text))
	{
		ELOG("PrefabSystem::LoadFromFile() failed to open file");
		return false;
	}

	if (!DeserializePrefab(text, _outPrefab))
	{
		ELOG("PrefabSystem::LoadFromFile() invalid prefab file");
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------
// ファイルを読み、シーンへ1体置く
// -------------------------------------------------------------------------------
GameObject* Editor::PrefabSystem::InstantiateFromFile(
	EditorContext&					_ctx,
	const std::filesystem::path&	_path)
{
	PrefabAsset prefab;

	if (!LoadFromFile(_path, prefab))
	{
		return nullptr;
	}

	auto pObject = Build(_ctx, prefab);
	if (pObject == nullptr)
	{
		return nullptr;
	}

	// 置く操作は、手で作った場合とまったく同じ経路を通す
	// こうしておくと、配置もそのままUndoで取り消せる
	return ObjectCommands::Spawn(_ctx, std::move(pObject), "プレファブの配置");
}

// -------------------------------------------------------------------------------
// プレファブファイルかどうか
// -------------------------------------------------------------------------------
bool Editor::PrefabSystem::IsPrefabPath(const std::filesystem::path& _path)
{
	if (!_path.has_extension())
	{
		return false;
	}

	// 拡張子の大文字小文字は問わない
	std::string extension = _path.extension().string();

	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](unsigned char _c) { return static_cast<char>(std::tolower(_c)); });

	return extension == kExtension;
}
