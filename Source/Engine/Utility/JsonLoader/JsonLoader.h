#pragma once
// -------------------------------------------------------------------------------
// JsonLoader class
// 
// 概要 : 
//	JSONファイルの読み込み・パースを人手に引き受けるユーティリティクラス
//	PipelineState / RootSignatureLayout などのJSON駆動の設定を読む各クラスから共通で利用する
// -------------------------------------------------------------------------------
class JsonLoader
{
public:

	// -------------------------------------------------------------------------------
	// @brief	JSONファイルを読み込んでパースする
	// 
	// @param[in]	_path		JSONファイルパス
	// @param[in]	_outJson	パース結果の格納先
	// @retval	true	成功
	// @retval	false	失敗
	// -------------------------------------------------------------------------------
	static bool Load(const std::wstring& _path, nlohmann::json& _outJson);

	// -------------------------------------------------------------------------------
	// @brief	JSONオブジェクトから任意型の値を安全に取得する
	//			キーが存在しない場合は_defaultValueを返す
	// 
	// @param[in]	_json			対象のJsonObject
	// @param[in]	_key			取得したいキー名
	// @param[in]	_defaultValue	キーが存在しない場合のデフォルト値
	// -------------------------------------------------------------------------------
	template<typename T>
	static T GetOrDefault(const nlohmann::json& _json, const std::string& _key, const T& _defaultValue)
	{
		return _json.value(_key, _defaultValue);
	}

	// -------------------------------------------------------------------------------
	// @brief	JSONオブジェクトから必須キーの値を取得する
	//			キーが存在しない場合はELOGを出してfalseを返す
	// 
	// @param[in]	_json		対象のJSONオブジェクト
	// @param[in]	_key		取得したいキー名
	// @param[in]	_outValue	取得した値の格納先
	// @retval	true	成功
	// @retval	false	失敗
	// -------------------------------------------------------------------------------
	template<typename T>
	static bool GetRequired(const nlohmann::json& _json, const std::string& _key, T& _outValue)
	{
		if (!_json.contains(_key))
		{
			return false;
		}
		_outValue = _json.at(_key).get<T>();
		return true;
	}

private:

	JsonLoader()	= default;
	~JsonLoader()	= default;
};