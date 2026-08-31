// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "GameObjectFactory.h"

#include <Engine/GameObject/GameObjectManager.h>
#include <Engine/GameObject/Components/TransformComponent/TransformComponent.h>
#include <Editor/Components/ComponentRegistry/ComponentRegistry.h>

namespace
{
	// -------------------------------------------------------------------------------
	// 同じ名前がすでに使われているかを調べる
	//
	// _pIgnore は「自分自身」を除外するために使う
	// 名前の変更で、変更前の自分と衝突して連番が付くのを防ぐ
	// -------------------------------------------------------------------------------
	bool IsNameUsed(
		const GameObjectManager&	_objects,
		const std::string&			_name,
		const GameObject*			_pIgnore)
	{
		const auto& objects = _objects.GetObjects();

		return std::any_of(objects.begin(), objects.end(),
			[&_name, _pIgnore](const std::unique_ptr<GameObject>& _object)
			{
				return _object != nullptr
					&& _object.get() != _pIgnore
					&& _object->GetName() == _name;
			});
	}
}

// -------------------------------------------------------------------------------
// 重複しない名前を作る
// -------------------------------------------------------------------------------
std::string Editor::GameObjectFactory::MakeUniqueName(
	const EditorContext&	_ctx,
	std::string_view		_baseName,
	const GameObject*		_pIgnore)
{
	const std::string baseName(_baseName);

	if (_ctx.pObjects == nullptr)
	{
		return baseName;
	}

	// そのままの名前が空いていれば、番号を足さない
	if (!IsNameUsed(*_ctx.pObjects, baseName, _pIgnore))
	{
		return baseName;
	}

	// "New Object (1)" のように連番を足していく
	// 上限は保険。ここに到達するほど同名が並ぶことは実際には起きない
	constexpr int kMaxAttempts = 10000;

	for (int suffix = 1; suffix < kMaxAttempts; ++suffix)
	{
		std::string candidate = baseName + " (" + std::to_string(suffix) + ")";

		if (!IsNameUsed(*_ctx.pObjects, candidate, _pIgnore))
		{
			return candidate;
		}
	}

	return baseName;
}

// -------------------------------------------------------------------------------
// 空のオブジェクトを作る（シーンにはまだ入れない）
// -------------------------------------------------------------------------------
std::unique_ptr<GameObject> Editor::GameObjectFactory::CreateEmptyDetached(
	const EditorContext&		_ctx,
	std::string_view			_baseName,
	const DirectX::XMFLOAT3&	_position)
{
	// -------------------------------------------------------------------------------
	// 基底クラスの GameObject をそのまま生成する
	//
	// 派生クラスを増やさず、必要な機能は Component で足していく設計
	// -------------------------------------------------------------------------------
	auto pObject = std::make_unique<GameObject>(MakeUniqueName(_ctx, _baseName));

	// -------------------------------------------------------------------------------
	// Transform は「シーン上のどこにあるか」を表す最低限の情報なので必ず付ける
	// これが無いとインスペクタで位置を編集できない
	// -------------------------------------------------------------------------------
	if (auto* pTransform = pObject->AddComponent<TransformComponent>())
	{
		pTransform->SetPosition(_position);
	}

	return pObject;
}

// -------------------------------------------------------------------------------
// 複製（シーンにはまだ入れない）
// -------------------------------------------------------------------------------
std::unique_ptr<GameObject> Editor::GameObjectFactory::CloneDetached(
	const EditorContext&	_ctx,
	const GameObject*		_pSource)
{
	if (_pSource == nullptr)
	{
		return nullptr;
	}

	// 名前は元の名前を基準にする。連番は MakeUniqueName が付ける
	auto pClone = std::make_unique<GameObject>(MakeUniqueName(_ctx, _pSource->GetName()));

	pClone->SetActive(_pSource->IsActive());

	// -------------------------------------------------------------------------------
	// コンポーネントの中身は ComponentRegistry が写す
	//
	// どこまで引き継げるかを1か所（登録表）に集約することで、
	// 複製・プレファブ・コンポーネント削除の取り消しが必ず同じ結果になる
	// -------------------------------------------------------------------------------
	ComponentRegistry::CopyComponents(*_pSource, *pClone);

	return pClone;
}

// -------------------------------------------------------------------------------
// シーンへ入れる
// -------------------------------------------------------------------------------
GameObject* Editor::GameObjectFactory::Attach(
	EditorContext&				_ctx,
	std::unique_ptr<GameObject> _pObject)
{
	if (_ctx.pObjects == nullptr || _pObject == nullptr)
	{
		// シーンが無い場合、ここで _pObject は破棄される
		// 呼び出し元は戻り値がnullptrなら「入らなかった」と判断する
		return nullptr;
	}

	return _ctx.pObjects->AddExisting(std::move(_pObject));
}

// -------------------------------------------------------------------------------
// シーンから外す（破棄はしない）
// -------------------------------------------------------------------------------
std::unique_ptr<GameObject> Editor::GameObjectFactory::Detach(
	EditorContext&	_ctx,
	GameObject*		_pObject)
{
	if (_ctx.pObjects == nullptr || _pObject == nullptr)
	{
		return nullptr;
	}

	return _ctx.pObjects->Detach(_pObject);
}

// -------------------------------------------------------------------------------
// まだシーンに存在するか
// -------------------------------------------------------------------------------
bool Editor::GameObjectFactory::IsAlive(const EditorContext& _ctx, const GameObject* _pObject)
{
	if (_ctx.pObjects == nullptr || _pObject == nullptr)
	{
		return false;
	}

	const auto& objects = _ctx.pObjects->GetObjects();

	return std::any_of(objects.begin(), objects.end(),
		[_pObject](const std::unique_ptr<GameObject>& _candidate)
		{
			return _candidate.get() == _pObject;
		});
}
