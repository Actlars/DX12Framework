// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "ComponentRegistry.h"

#include <Engine/GameObject/GameObject.h>
#include <Engine/GameObject/Components/TransformComponent/TransformComponent.h>
#include <Engine/GameObject/Components/MeshComponent/MeshComponent.h>
#include <Engine/GameObject/Components/MeshletComponent/MeshletComponent.h>
#include <Engine/GameObject/Components/HitComponent/BoxCollisionComponent/BoxCollisionComponent.h>

// -------------------------------------------------------------------------------
// 型ごとの手順
//
// ここに書くのは「データとしてどう扱うか」だけ
// エディタでどう見せるかは Editor::ComponentInspectors 側にある
// -------------------------------------------------------------------------------
namespace
{
	void LoadBoxCollision(GameObject& _object, const nlohmann::json& _json);
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

	// 読み書きのためだけにconstを外す小さな入り口
	// 「値を読むだけ」という意図を1か所に閉じ込める
	template<typename T>
	T* GetForRead(const GameObject& _object)
	{
		return const_cast<GameObject&>(_object).GetComponent<T>();
	}

	// -------------------------------------------------------------------------------
	// JSONとの相互変換で使う小物
	//
	//	どれも「キーが無ければ既定値のまま」という方針
	//	項目が増えても、古いプレファブやシーンがそのまま開けるようにするため
	// -------------------------------------------------------------------------------
	nlohmann::json ToJson(const DirectX::XMFLOAT3& _value)
	{
		return nlohmann::json::array({ _value.x, _value.y, _value.z });
	}

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

	std::string ReadString(const nlohmann::json& _json, const char* _key)
	{
		if (!_json.contains(_key) || !_json.at(_key).is_string())
		{
			return {};
		}

		return _json.at(_key).get<std::string>();
	}

	uint32_t ReadUInt(const nlohmann::json& _json, const char* _key, uint32_t _default)
	{
		if (!_json.contains(_key) || !_json.at(_key).is_number_unsigned())
		{
			return _default;
		}

		return _json.at(_key).get<uint32_t>();
	}

	// -------------------------------------------------------------------------------
	// TransformComponent
	// -------------------------------------------------------------------------------
	void SaveTransform(const GameObject& _object, nlohmann::json& _outJson)
	{
		const auto* pTransform = GetForRead<TransformComponent>(_object);
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
	//	保存するのは「どのモデルの何番目か」と表示フラグだけ
	//	GPUリソースそのものはModelLibraryが持ち、
	//	結び付ける作業はシーンが毎フレーム行う
	// -------------------------------------------------------------------------------
	void SaveMesh(const GameObject& _object, nlohmann::json& _outJson)
	{
		const auto* pMesh = GetForRead<MeshComponent>(_object);
		if (pMesh == nullptr)
		{ return; }

		_outJson["Model"]	= pMesh->GetModelKey();
		_outJson["Part"]	= pMesh->GetPartIndex();

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

		pMesh->SetModelRequest(ReadString(_json, "Model"), ReadUInt(_json, "Part", 0));
		pMesh->SetVisible(ReadBool(_json, "Visible", true));
	}

	// -------------------------------------------------------------------------------
	// MeshletComponent
	// -------------------------------------------------------------------------------
	void SaveMeshlet(const GameObject& _object, nlohmann::json& _outJson)
	{
		const auto* pMeshlet = GetForRead<MeshletComponent>(_object);
		if (pMeshlet == nullptr)
		{ return; }

		_outJson["Model"] = pMeshlet->GetModelKey();

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

		pMeshlet->SetModelRequest(ReadString(_json, "Model"));
		pMeshlet->SetVisible(ReadBool(_json, "Visible", true));
	}

	void SaveBoxCollision(const GameObject& _object, nlohmann::json& _outJson)
	{
		const auto* pBoxCollision = GetForRead<BoxCollisionComponent>(_object);
		if (pBoxCollision == nullptr)
		{ return; }

		_outJson["DebugLineEnable"] = pBoxCollision->IsDebugDrawEnabled();
	}

	void LoadBoxCollision(GameObject& _object, const nlohmann::json& _json)
	{
		auto* pBoxCollision = _object.GetComponent<BoxCollisionComponent>();
		if (pBoxCollision == nullptr)
		{ return; }

		pBoxCollision->SetDebugDrawEnabled(ReadBool(_json, "Visible", true));
	}

	// -------------------------------------------------------------------------------
	// 登録表
	//
	//	新しいコンポーネントを対応させるときは、ここへ1項目足す
	//	関数staticにしているのは、初回参照時に一度だけ作られることを保証するため
	//	（起動時の静的初期化の順番に依存しない）
	// -------------------------------------------------------------------------------
	const std::vector<ComponentTypeInfo>& GetTable()
	{
		static const std::vector<ComponentTypeInfo> s_Table =
		{
			{
				"TransformComponent",
				&HasComponent<TransformComponent>,
				&AddComponent<TransformComponent>,
				&RemoveComponent<TransformComponent>,
				&SaveTransform,
				&LoadTransform,
			},
			{
				"MeshComponent",
				&HasComponent<MeshComponent>,
				&AddComponent<MeshComponent>,
				&RemoveComponent<MeshComponent>,
				&SaveMesh,
				&LoadMesh,
			},
			{
				"MeshletComponent",
				&HasComponent<MeshletComponent>,
				&AddComponent<MeshletComponent>,
				&RemoveComponent<MeshletComponent>,
				&SaveMeshlet,
				&LoadMeshlet,
			},
			{
				"BoxCollisionComponent",
				&HasComponent<BoxCollisionComponent>,
				&AddComponent<BoxCollisionComponent>,
				&RemoveComponent<BoxCollisionComponent>,
				&SaveBoxCollision,
				&LoadBoxCollision,
			}
		};

		return s_Table;
	}
}

// -------------------------------------------------------------------------------
// 全種別
// -------------------------------------------------------------------------------
const std::vector<ComponentTypeInfo>& ComponentRegistry::GetAll()
{
	return GetTable();
}

// -------------------------------------------------------------------------------
// 内部名から種別を引く
// -------------------------------------------------------------------------------
const ComponentTypeInfo* ComponentRegistry::Find(std::string_view _typeName)
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
nlohmann::json ComponentRegistry::CaptureComponent(
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
void ComponentRegistry::RestoreComponent(
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
void ComponentRegistry::CopyComponents(
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
		// 複製・プレファブ・シーンで結果が食い違わないようにするため
		RestoreComponent(_destination, info, CaptureComponent(_source, info));
	}
}
