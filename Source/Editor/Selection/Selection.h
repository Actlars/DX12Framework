#pragma once

class GameObject;

namespace Editor
{
	// -------------------------------------------------------------------------------
	// SelectionType : enum
	//
	// 概要 :
	//	いま何が選ばれているか
	//	インスペクタは、この種類だけを見て「何を表示するか」を切り替える
	// -------------------------------------------------------------------------------
	enum class SelectionType : uint8_t
	{
		None,			// 何も選んでいない
		SceneObject,	// ヒエラルキー上のGameObject
		Asset,			// コンテンツブラウザ上のファイル/フォルダ
	};

	// -------------------------------------------------------------------------------
	// Selection class
	//
	// 概要 :
	//	エディタ全体でただ1つの「選択状態」
	//
	//	ヒエラルキーとコンテンツブラウザが選択を書き込み、
	//	インスペクタがそれを読んで中身を表示する
	//	パネル同士が直接参照し合わないようにするための、共有の置き場所
	//
	//	GameObjectは生ポインタで持つため、シーンから消えた場合に備えて
	//	毎フレームValidateで生存を確認する
	// -------------------------------------------------------------------------------
	class Selection
	{
	public:

		SelectionType GetType() const { return m_Type; }

		// -------------------------------------------------------------------------------
		// シーン上のオブジェクト
		// -------------------------------------------------------------------------------
		void SelectObject(GameObject* _pObject)
		{
			m_Type		= (_pObject != nullptr) ? SelectionType::SceneObject : SelectionType::None;
			m_pObject	= _pObject;
			m_AssetPath.clear();
		}

		GameObject* GetObject() const
		{
			return (m_Type == SelectionType::SceneObject) ? m_pObject : nullptr;
		}

		bool IsObjectSelected(const GameObject* _pObject) const
		{
			return m_Type == SelectionType::SceneObject && m_pObject == _pObject;
		}

		// -------------------------------------------------------------------------------
		// アセット(ファイル/フォルダ)
		// -------------------------------------------------------------------------------
		void SelectAsset(const std::filesystem::path& _path)
		{
			m_Type		= SelectionType::Asset;
			m_AssetPath	= _path;
			m_pObject	= nullptr;
		}

		const std::filesystem::path& GetAssetPath() const { return m_AssetPath; }

		bool IsAssetSelected(const std::filesystem::path& _path) const
		{
			return m_Type == SelectionType::Asset && m_AssetPath == _path;
		}

		void Clear()
		{
			m_Type = SelectionType::None;
			m_pObject = nullptr;
			m_AssetPath.clear();
		}

		// -------------------------------------------------------------------------------
		// @brief	選択中のオブジェクトがまだ生きているかを確認し、消えていれば選択を解除する
		//
		//	生ポインタを保持しているため、フレームの先頭で必ず1回呼ぶ
		//
		// @param[in]	_isAlive	そのポインタがまだ有効かを答える関数
		// -------------------------------------------------------------------------------
		void Validate(const std::function<bool(const GameObject*)>& _isAlive)
		{
			if (m_Type != SelectionType::SceneObject)
			{ return; }

			if (m_pObject == nullptr || !_isAlive(m_pObject))
			{
				Clear();
			}
		}

	private:

		SelectionType			m_Type		= SelectionType::None;
		GameObject*				m_pObject	= nullptr;	// 所有権なし
		std::filesystem::path	m_AssetPath;
	};
}
