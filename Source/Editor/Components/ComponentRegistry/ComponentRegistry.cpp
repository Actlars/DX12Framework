// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "ComponentRegistry.h"

#include <Engine/GameObject/GameObject.h>
#include <Engine/GameObject/Components/TransformComponent/TransformComponent.h>
#include <Engine/GameObject/Components/MeshComponent/MeshComponent.h>
#include <Engine/GameObject/Components/MeshletComponent/MeshletComponent.h>

#include <Editor/Components/ComponentInspectors/ComponentInspectors.h>

// -------------------------------------------------------------------------------
// 型ごとの手順
//
// ここに書くのは「データとしてどう扱うか」だけ
// 画面にどう出すかは ComponentInspectors 側にある
// -------------------------------------------------------------------------------
namespace
{
	// -------------------------------------------------------------------------------
	// 型に依存しない共通の手順
	//
	//	Has / Add / Remove はどの型でも中身が同じで、違うのは型引数だけ
	//	テンプレートにしておくことで、表への登録は型名を書くだけで済む
	// -------------------------------------------------------------------------------
	template<typename T>
	bool HasComponent(const GameObject& _object)
	{
		// GetComponentはconstメンバではないが、中身を書き換えないため
		// 呼び出しのためだけに一時的にconstを外す
		return const_cast<GameObject&>(_object).GetComponent<T>() != nullptr;
	}

	template<typename T>
	Component* AddComponent(GameObject& _object)
	{
		return _object.AddComponent<T>();
	}

	template<typename T>
	bool RemoveComponent(GameObject& _object)
	{
		return _object.RemoveComponent<T>();
	}

	// -------------------------------------------------------------------------------
	// JSONとの相互変換で使う小物
	// -------------------------------------------------------------------------------
	nlohmann::json ToJson(const DirectX::XMFLOAT3& _value)
	{
		return nlohmann::json::array({ _value.x, _value.y, _value.z });
	}

	// キーが無い・型が違う場合は既定値のままにする
	// 項目が増えても古いプレファブがそのまま開けるようにするため
	DirectX::XMFLOAT3 ReadFloat3(
		const nlohmann::json&		_json,
		const char*					_key,
		const DirectX::XMFLOAT3&	_default)
	{
		if (!_json.contains(_key) || !_json.at(_key).is_array() || _json.at(_key).size() < 3)
		{
			return _default;
		}

		const auto& array = _json.at(_key);

		return
		{
			array[0].get<float>(),
			array[1].get<float>(),
			array[2].get<float>()
		};
	}

	bool ReadBool(const nlohmann::json& _json, const char* _key, bool _default)
	{
		if (!_json.contains(_key) || !_json.at(_key).is_boolean())
		{
			return _default;
		}

		return _json.at(_key).get<bool>();
	}

	// -------------------------------------------------------------------------------
	// TransformComponent
	// -------------------------------------------------------------------------------
	void SaveTransform(const GameObject& _object, nlohmann::json& _outJson)
	{
		const auto* pTransform = const_cast<GameObject&>(_object).GetComponent<TransformComponent>();
		if (pTransform == nullptr)
		{ return; }

		_outJson["Position"] = ToJson(pTransform->GetPosition());
		_outJson["Rotation"] = ToJson(pTransform->GetRotation());
		_outJson["Scale"]    = ToJson(pTransform->GetScale());
	}

	void LoadTransform(GameObject& _object, const nlohmann::json& _json)
	{
		auto* pTransform = _object.GetComponent<TransformComponent>();
		if (pTransform == nullptr)
		{ return; }

		pTransform->SetPosition(ReadFloat3(_json, "Position", { 0.0f, 0.0f, 0.0f }));
		pTransform->SetRotation(ReadFloat3(_json, "Rotation", { 0.0f, 0.0f, 0.0f }));
		pTransform->SetScale   (ReadFloat3(_json, "Scale",    { 1.0f, 1.0f, 1.0f }));
	}

	// -------------------------------------------------------------------------------
	// MeshComponent
	//
	//	描画するメッシュそのものは、シーン側がモデルを読み込んで割り当てる
	//	エディタから作れるのは「まだメッシュが入っていない入れ物」までで、
	//	保存できるのも表示フラグだけになる
	// -------------------------------------------------------------------------------
	void SaveMesh(const GameObject& _object, nlohmann::json& _outJson)
	{
		const auto* pMesh = const_cast<GameObject&>(_object).GetComponent<MeshComponent>();
		if (pMesh == nullptr)
		{ return; }

		// メッシュが未設定のときIsVisible()は常にfalseを返すため、
		// その状態の値を保存すると次に開いたとき非表示になってしまう
		if (pMesh->HasMesh())
		{
			_outJson["Visible"] = pMesh->IsVisible();
		}
	}

	void LoadMesh(GameObject& _object, const nlohmann::json& _json)
	{
		auto* pMesh = _object.GetComponent<MeshComponent>();
		if (pMesh == nullptr)
		{ return; }

		pMesh->SetVisible(ReadBool(_json, "Visible", true));
	}

	// -------------------------------------------------------------------------------
	// MeshletComponent
	// -------------------------------------------------------------------------------
	void SaveMeshlet(const GameObject& _object, nlohmann::json& _outJson)
	{
		const auto* pMeshlet = const_cast<GameObject&>(_object).GetComponent<MeshletComponent>();
		if (pMeshlet == nullptr)
		{ return; }

		if (pMeshlet->GetMeshCount() > 0)
		{
			_outJson["Visible"] = pMeshlet->IsVisible();
		}
	}

	void LoadMeshlet(GameObject& _object, const nlohmann::json& _json)
	{
		auto* pMeshlet = _object.GetComponent<MeshletComponent>();
		if (pMeshlet == nullptr)
		{ return; }

		pMeshlet->SetVisible(ReadBool(_json, "Visible", true));
	}

	// -------------------------------------------------------------------------------
	// 登録表
	//
	//	新しいコンポーネントをエディタ対応させるときは、ここへ1項目足す
	//	関数staticにしているのは、初回参照時に一度だけ作られることを保証するため
	//	（起動時の静的初期化の順番に依存しない）
	// -------------------------------------------------------------------------------
	const std::vector<Editor::ComponentTypeInfo>& GetTable()
	{
		static const std::vector<Editor::ComponentTypeInfo> s_Table =
		{
			{
				"TransformComponent",
				"Transform",
				false,	// 位置情報は描画コンポーネントの前提になるため外させない
				&HasComponent<TransformComponent>,
				&AddComponent<TransformComponent>,
				&RemoveComponent<TransformComponent>,
				&SaveTransform,
				&LoadTransform,
				&Editor::ComponentInspectors::DrawTransform,
			},
			{
				"MeshComponent",
				"Mesh",
				true,
				&HasComponent<MeshComponent>,
				&AddComponent<MeshComponent>,
				&RemoveComponent<MeshComponent>,
				&SaveMesh,
				&LoadMesh,
				&Editor::ComponentInspectors::DrawMesh,
			},
			{
				"MeshletComponent",
				"Meshlet",
				true,
				&HasComponent<MeshletComponent>,
				&AddComponent<MeshletComponent>,
				&RemoveComponent<MeshletComponent>,
				&SaveMeshlet,
				&LoadMeshlet,
				&Editor::ComponentInspectors::DrawMeshlet,
			},
		};

		return s_Table;
	}
}

// -------------------------------------------------------------------------------
// 全種別
// -------------------------------------------------------------------------------
const std::vector<Editor::ComponentTypeInfo>& Editor::ComponentRegistry::GetAll()
{
	return GetTable();
}

// -------------------------------------------------------------------------------
// 内部名から種別を引く
// -------------------------------------------------------------------------------
const Editor::ComponentTypeInfo* Editor::ComponentRegistry::Find(std::string_view _typeName)
{
	for (const ComponentTypeInfo& info : GetTable())
	{
		if (info.TypeName == _typeName)
		{
			return &info;
		}
	}

	return nullptr;
}

// -------------------------------------------------------------------------------
// 1つのコンポーネントの現在値を写し取る
// -------------------------------------------------------------------------------
nlohmann::json Editor::ComponentRegistry::CaptureComponent(
	const GameObject&			_object,
	const ComponentTypeInfo&	_typeInfo)
{
	nlohmann::json json = nlohmann::json::object();

	if (_typeInfo.Save != nullptr)
	{
		_typeInfo.Save(_object, json);
	}

	return json;
}

// -------------------------------------------------------------------------------
// 写し取った値からコンポーネントを復元する
// -------------------------------------------------------------------------------
void Editor::ComponentRegistry::RestoreComponent(
	GameObject&					_object,
	const ComponentTypeInfo&	_typeInfo,
	const nlohmann::json&		_json)
{
	if (_typeInfo.Add == nullptr)
	{ return; }

	// すでに付いている場合、Addは既存のものを返すため二重にはならない
	_typeInfo.Add(_object);

	if (_typeInfo.Load != nullptr)
	{
		_typeInfo.Load(_object, _json);
	}
}

// -------------------------------------------------------------------------------
// 登録済みコンポーネントをまるごと写す
// -------------------------------------------------------------------------------
void Editor::ComponentRegistry::CopyComponents(
	const GameObject&	_source,
	GameObject&			_destination)
{
	for (const ComponentTypeInfo& info : GetTable())
	{
		if (info.Has == nullptr || !info.Has(_source))
		{
			continue;	// 元が持っていないものは写さない
		}

		// 保存 → 復元 の経路をそのまま使う
		// 複製とプレファブで結果が食い違わないようにするため
		RestoreComponent(_destination, info, CaptureComponent(_source, info));
	}
}
