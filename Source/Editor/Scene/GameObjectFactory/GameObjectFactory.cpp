// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "GameObjectFactory.h"

#include <Engine/GameObject/GameObjectManager.h>
#include <Engine/GameObject/Components/TransformComponent/TransformComponent.h>

namespace
{
	// -------------------------------------------------------------------------------
	// 同じ名前がすでに使われているかを調べる
	// -------------------------------------------------------------------------------
	bool IsNameUsed(const GameObjectManager& _objects, const std::string& _name)
	{
		const auto& objects = _objects.GetObjects();

		return std::any_of(objects.begin(), objects.end(),
			[&_name](const std::unique_ptr<GameObject>& _object)
			{
				return _object != nullptr && _object->GetName() == _name;
			});
	}
}

// -------------------------------------------------------------------------------
// 重複しない名前を作る
// -------------------------------------------------------------------------------
std::string Editor::GameObjectFactory::MakeUniqueName(
	const EditorContext& _ctx, std::string_view _baseName)
{
	const std::string baseName(_baseName);

	if (_ctx.pObjects == nullptr)
	{
		return baseName;
	}

	// そのままの名前が空いていれば、番号を足さない
	if (!IsNameUsed(*_ctx.pObjects, baseName))
	{
		return baseName;
	}

	// "New Object (1)" のように連番を足していく
	// 上限は保険。ここに到達するほど同名が並ぶことは実際には起きない
	constexpr int kMaxAttempts = 10000;

	for (int suffix = 1; suffix < kMaxAttempts; ++suffix)
	{
		std::string candidate = baseName + " (" + std::to_string(suffix) + ")";

		if (!IsNameUsed(*_ctx.pObjects, candidate))
		{
			return candidate;
		}
	}

	return baseName;
}

// -------------------------------------------------------------------------------
// 空のオブジェクトを作る
// -------------------------------------------------------------------------------
GameObject* Editor::GameObjectFactory::CreateEmpty(
	EditorContext&				_ctx,
	std::string_view			_baseName,
	const DirectX::XMFLOAT3&	_position)
{
	if (_ctx.pObjects == nullptr)
	{
		return nullptr;	// シーンが読み込まれていない
	}

	// -------------------------------------------------------------------------------
	// 基底クラスの GameObject をそのまま生成する
	//
	// 派生クラスを増やさず、必要な機能は Component で足していく設計
	// 所有権は GameObjectManager が持ち、ここで受け取るのは参照だけ
	// -------------------------------------------------------------------------------
	GameObject* pObject = _ctx.pObjects->Add<GameObject>(MakeUniqueName(_ctx, _baseName));

	if (pObject == nullptr)
	{
		return nullptr;
	}

	// -------------------------------------------------------------------------------
	// Transform は「シーン上のどこにあるか」を表す最低限の情報なので必ず付ける
	// これが無いとインスペクタで位置を編集できない
	// -------------------------------------------------------------------------------
	if (auto* pTransform = pObject->AddComponent<TransformComponent>())
	{
		pTransform->SetPosition(_position);
	}

	// 作った直後に編集へ移れるよう、そのまま選択状態にする
	if (_ctx.pSelection != nullptr)
	{
		_ctx.pSelection->SelectObject(pObject);
	}

	return pObject;
}

// -------------------------------------------------------------------------------
// 複製
// -------------------------------------------------------------------------------
GameObject* Editor::GameObjectFactory::Duplicate(EditorContext& _ctx, const GameObject* _pSource)
{
	if (_ctx.pObjects == nullptr || _pSource == nullptr)
	{
		return nullptr;
	}

	// 複製元の位置を引き継ぐ。Transform が無い場合は原点に置く
	DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 rotation{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 scale   { 1.0f, 1.0f, 1.0f };

	// GetComponent は非 const のため、複製元を一時的に非 const として扱う
	// 値を読むだけで書き換えはしない
	auto* pMutableSource = const_cast<GameObject*>(_pSource);

	if (const auto* pSourceTransform = pMutableSource->GetComponent<TransformComponent>())
	{
		position = pSourceTransform->GetPosition();
		rotation = pSourceTransform->GetRotation();
		scale    = pSourceTransform->GetScale();
	}

	// 名前は元の名前を基準にする。連番は MakeUniqueName が付ける
	GameObject* pCopy = CreateEmpty(_ctx, _pSource->GetName(), position);

	if (pCopy == nullptr)
	{
		return nullptr;
	}

	if (auto* pCopyTransform = pCopy->GetComponent<TransformComponent>())
	{
		pCopyTransform->SetRotation(rotation);
		pCopyTransform->SetScale(scale);
	}

	return pCopy;
}

// -------------------------------------------------------------------------------
// 削除
// -------------------------------------------------------------------------------
bool Editor::GameObjectFactory::Destroy(EditorContext& _ctx, GameObject* _pTarget)
{
	if (_ctx.pObjects == nullptr || _pTarget == nullptr)
	{
		return false;
	}

	// 実際に消えるのはフレーム末。Update / Draw の途中でリストが変わらないようにするため
	_ctx.pObjects->Remove(_pTarget);

	// 消える対象を選んだままにしない
	if (_ctx.pSelection != nullptr && _ctx.pSelection->GetObject() == _pTarget)
	{
		_ctx.pSelection->Clear();
	}

	return true;
}
