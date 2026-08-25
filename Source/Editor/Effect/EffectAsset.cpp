// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "EffectAsset.h"

// -------------------------------------------------------------------------------
// .effectファイルの読み書き
//
// 人が読めるJSONにしておくことで、差分が追えるようにしている
// 読み込み時は「見つかったキーだけ上書きする」方針にして、
// 項目が増えても古いファイルがそのまま開けるようにする
// -------------------------------------------------------------------------------
namespace
{
	// キーが存在し、かつ型が合うときだけ値を取り出す
	template<typename T>
	void ReadValue(const nlohmann::json& _json, const char* _key, T& _outValue)
	{
		if (!_json.contains(_key))
		{ return; }

		try
		{
			_outValue = _json.at(_key).get<T>();
		}
		catch (const nlohmann::json::exception&)
		{
			// 型が違う場合は既定値のままにする。壊れたファイルでも開けるようにするため
		}
	}

	void ReadFloat4(const nlohmann::json& _json, const char* _key, DirectX::XMFLOAT4& _outValue)
	{
		if (!_json.contains(_key) || !_json.at(_key).is_array() || _json.at(_key).size() < 4)
		{ return; }

		const auto& array = _json.at(_key);
		_outValue =
		{
			array[0].get<float>(), array[1].get<float>(),
			array[2].get<float>(), array[3].get<float>()
		};
	}
}

// -------------------------------------------------------------------------------
// 書き出し
// -------------------------------------------------------------------------------
std::string Editor::SerializeEffect(const EffectAsset& _effect)
{
	nlohmann::json json;

	json["Name"]				= _effect.Name;

	json["SpawnRate"]			= _effect.SpawnRate;
	json["Shape"]				= static_cast<int>(_effect.Shape);
	json["ShapeRadius"]			= _effect.ShapeRadius;

	json["LifeTime"]			= _effect.LifeTime;
	json["LifeTimeRandom"]		= _effect.LifeTimeRandom;

	json["InitialSpeed"]		= _effect.InitialSpeed;
	json["InitialSpeedRandom"]	= _effect.InitialSpeedRandom;
	json["SpreadAngle"]			= _effect.SpreadAngle;
	json["Gravity"]				= _effect.Gravity;
	json["Drag"]				= _effect.Drag;

	json["StartSize"]			= _effect.StartSize;
	json["EndSize"]				= _effect.EndSize;

	json["StartColor"] = { _effect.StartColor.x, _effect.StartColor.y, _effect.StartColor.z, _effect.StartColor.w };
	json["EndColor"]   = { _effect.EndColor.x,   _effect.EndColor.y,   _effect.EndColor.z,   _effect.EndColor.w   };

	json["Looping"]				= _effect.Looping;
	json["Duration"]			= _effect.Duration;
	json["MaxParticles"]		= _effect.MaxParticles;

	return json.dump(4);
}

// -------------------------------------------------------------------------------
// 読み込み
// -------------------------------------------------------------------------------
bool Editor::DeserializeEffect(std::string_view _json, EffectAsset& _outEffect)
{
	nlohmann::json json = nlohmann::json::parse(_json, nullptr, false);
	if (json.is_discarded() || !json.is_object())
	{
		return false;	// JSONとして壊れている
	}

	ReadValue(json, "Name",					_outEffect.Name);

	ReadValue(json, "SpawnRate",			_outEffect.SpawnRate);

	int shape = static_cast<int>(_outEffect.Shape);
	ReadValue(json, "Shape", shape);
	_outEffect.Shape = static_cast<EmitterShape>(std::clamp(shape, 0, 2));

	ReadValue(json, "ShapeRadius",			_outEffect.ShapeRadius);

	ReadValue(json, "LifeTime",				_outEffect.LifeTime);
	ReadValue(json, "LifeTimeRandom",		_outEffect.LifeTimeRandom);

	ReadValue(json, "InitialSpeed",			_outEffect.InitialSpeed);
	ReadValue(json, "InitialSpeedRandom",	_outEffect.InitialSpeedRandom);
	ReadValue(json, "SpreadAngle",			_outEffect.SpreadAngle);
	ReadValue(json, "Gravity",				_outEffect.Gravity);
	ReadValue(json, "Drag",					_outEffect.Drag);

	ReadValue(json, "StartSize",			_outEffect.StartSize);
	ReadValue(json, "EndSize",				_outEffect.EndSize);

	ReadFloat4(json, "StartColor",			_outEffect.StartColor);
	ReadFloat4(json, "EndColor",			_outEffect.EndColor);

	ReadValue(json, "Looping",				_outEffect.Looping);
	ReadValue(json, "Duration",				_outEffect.Duration);
	ReadValue(json, "MaxParticles",			_outEffect.MaxParticles);

	return true;
}
