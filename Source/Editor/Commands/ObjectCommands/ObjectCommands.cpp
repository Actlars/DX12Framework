// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "ObjectCommands.h"

#include <Engine/GameObject/GameObject.h>
#include <Engine/GameObject/Components/TransformComponent/TransformComponent.h>

#include <Engine/GameObject/GameObjectManager.h>
#include <Engine/GameObject/GameObjectFactory/GameObjectFactory.h>

#include <Editor/Commands/CommandHistory/CommandHistory.h>

// -------------------------------------------------------------------------------
// コマンドの実装
//
// 外へ出す必要がないため、無名名前空間に閉じ込めている
// 画面側は ObjectCommands の関数だけを呼び、どんなクラスがあるかは知らなくてよい
// -------------------------------------------------------------------------------
namespace
{
	using namespace Editor;

	// -------------------------------------------------------------------------------
	// 対象がまだシーンに居るか
	//
	// 履歴は生ポインタを持つため、触る前に必ず確かめる
	// シーンが無い場合もfalseになる
	// -------------------------------------------------------------------------------
	bool IsTargetAlive(const EditorContext& _ctx, const GameObject* _pObject)
	{
		return _ctx.pObjects != nullptr
			&& GameObjectFactory::IsAlive(*_ctx.pObjects, _pObject);
	}

	// -------------------------------------------------------------------------------
	// ObjectLifetimeCommand class
	//
	// 概要 :
	//	オブジェクトをシーンへ「置く」「取り除く」操作
	//
	//	作成と削除は同じ操作の向き違いでしかない
	//		作成 : 実行=置く   / 取り消し=取り除く
	//		削除 : 実行=取り除く / 取り消し=置く
	//	2つのクラスに分けると、片方だけ直したときに挙動がずれる
	//	そこで向きだけを持たせて1つにまとめている
	//
	//	シーンの外にあるあいだ、実体はこのコマンドが預かる（m_pHeld）
	//	そのため取り消しても同じオブジェクトがそのまま戻り、
	//	IDもコンポーネントの状態も変わらない
	// -------------------------------------------------------------------------------
	class ObjectLifetimeCommand : public IEditorCommand
	{
	public:

		enum class Mode
		{
			Create,		// 実行でシーンへ置く
			Destroy,	// 実行でシーンから取り除く
		};

		// -------------------------------------------------------------------------------
		// @param[in]	_mode		操作の向き
		// @param[in]	_pHeld		Createのとき、置く対象（所有権を渡す）
		// @param[in]	_pTarget	Destroyのとき、取り除く対象（シーン上にあるもの）
		// @param[in]	_label		履歴に表示する操作名
		// -------------------------------------------------------------------------------
		ObjectLifetimeCommand(
			Mode						_mode,
			std::unique_ptr<GameObject>	_pHeld,
			GameObject*					_pTarget,
			std::string					_label)
			: m_Mode	(_mode)
			, m_pHeld	(std::move(_pHeld))
			, m_pObject	(m_pHeld != nullptr ? m_pHeld.get() : _pTarget)
			, m_Label	(std::move(_label))
		{ /* DO_NOTHING */ }

		bool Execute(EditorContext& _ctx) override
		{
			return (m_Mode == Mode::Create) ? Spawn(_ctx) : Despawn(_ctx);
		}

		void Undo(EditorContext& _ctx) override
		{
			if (m_Mode == Mode::Create)	{ Despawn(_ctx); }
			else						{ Spawn(_ctx); }
		}

		std::string_view GetLabel() const override { return m_Label; }

	private:

		// -------------------------------------------------------------------------------
		// 預かっている実体をシーンへ戻す
		// -------------------------------------------------------------------------------
		bool Spawn(EditorContext& _ctx)
		{
			// すでにシーンにある、または預かっていない場合は何もしない
			if (m_pHeld == nullptr)
			{ return false; }

			// シーンが無いときにAttachへ渡すと実体が捨てられてしまうため、先に確認する
			if (_ctx.pObjects == nullptr)
			{ return false; }

			// 所有権をシーンへ渡す。以降の実体の寿命はシーンが持つ
			GameObject* pAttached = _ctx.pObjects->AddExisting(std::move(m_pHeld));
			if (pAttached == nullptr)
			{ return false; }

			// 置いた直後にインスペクタで編集できるよう、そのまま選択状態にする
			if (_ctx.pSelection != nullptr)
			{
				_ctx.pSelection->SelectObject(pAttached);
			}

			return true;
		}

		// -------------------------------------------------------------------------------
		// シーンから取り除き、実体を預かる
		// -------------------------------------------------------------------------------
		bool Despawn(EditorContext& _ctx)
		{
			if (m_pObject == nullptr)
			{ return false; }

			if (_ctx.pObjects == nullptr)
			{ return false; }

			// 破棄せずに受け取る。これで取り消したときに同じものを戻せる
			std::unique_ptr<GameObject> pDetached = _ctx.pObjects->Detach(m_pObject);
			if (pDetached == nullptr)
			{ return false; }	// すでにシーンに無い

			m_pHeld = std::move(pDetached);

			// 消えたものを選んだままにしない
			if (_ctx.pSelection != nullptr && _ctx.pSelection->GetObject() == m_pObject)
			{
				_ctx.pSelection->Clear();
			}

			return true;
		}

		Mode						m_Mode;
		std::unique_ptr<GameObject>	m_pHeld;	// シーンの外にあるあいだの所有権
		GameObject*					m_pObject;	// 対象を指し続ける（実体は常にどこかに居る）
		std::string					m_Label;
	};

	// -------------------------------------------------------------------------------
	// RenameObjectCommand class
	//
	//	名前を入れ替えるだけの操作
	//	戻すべき値が1つなので、実行前の名前を持っておくだけで足りる
	// -------------------------------------------------------------------------------
	class RenameObjectCommand : public IEditorCommand
	{
	public:

		RenameObjectCommand(GameObject* _pTarget, std::string _before, std::string _after)
			: m_pObject	(_pTarget)
			, m_Before	(std::move(_before))
			, m_After	(std::move(_after))
		{ /* DO_NOTHING */ }

		bool Execute(EditorContext& _ctx) override { return Apply(_ctx, m_After); }
		void Undo	(EditorContext& _ctx) override { Apply(_ctx, m_Before); }

		std::string_view GetLabel() const override { return "名前の変更"; }

	private:

		bool Apply(EditorContext& _ctx, const std::string& _name)
		{
			if (!IsTargetAlive(_ctx, m_pObject))
			{ return false; }

			m_pObject->SetName(_name);
			return true;
		}

		GameObject*	m_pObject;
		std::string	m_Before;
		std::string	m_After;
	};

	// -------------------------------------------------------------------------------
	// SetActiveCommand class
	// -------------------------------------------------------------------------------
	class SetActiveCommand : public IEditorCommand
	{
	public:

		SetActiveCommand(GameObject* _pTarget, bool _before, bool _after)
			: m_pObject	(_pTarget)
			, m_Before	(_before)
			, m_After	(_after)
		{ /* DO_NOTHING */ }

		bool Execute(EditorContext& _ctx) override { return Apply(_ctx, m_After); }
		void Undo	(EditorContext& _ctx) override { Apply(_ctx, m_Before); }

		std::string_view GetLabel() const override { return "表示状態の変更"; }

	private:

		bool Apply(EditorContext& _ctx, bool _active)
		{
			if (!IsTargetAlive(_ctx, m_pObject))
			{ return false; }

			m_pObject->SetActive(_active);
			return true;
		}

		GameObject*	m_pObject;
		bool		m_Before;
		bool		m_After;
	};

	// -------------------------------------------------------------------------------
	// SetTransformCommand class
	//
	//	位置・回転・大きさをまとめて1つの操作として扱う
	//	3つを別々のコマンドにすると、1回のドラッグで3回Undoが必要になってしまう
	// -------------------------------------------------------------------------------
	class SetTransformCommand : public IEditorCommand
	{
	public:

		SetTransformCommand(
			GameObject*				_pTarget,
			const TransformValues&	_before,
			const TransformValues&	_after)
			: m_pObject	(_pTarget)
			, m_Before	(_before)
			, m_After	(_after)
		{ /* DO_NOTHING */ }

		bool Execute(EditorContext& _ctx) override { return Apply(_ctx, m_After); }
		void Undo	(EditorContext& _ctx) override { Apply(_ctx, m_Before); }

		std::string_view GetLabel() const override { return "Transformの変更"; }

	private:

		bool Apply(EditorContext& _ctx, const TransformValues& _values)
		{
			if (!IsTargetAlive(_ctx, m_pObject))
			{ return false; }

			ObjectCommands::ApplyTransform(*m_pObject, _values);
			return true;
		}

		GameObject*		m_pObject;
		TransformValues	m_Before;
		TransformValues	m_After;
	};

	// -------------------------------------------------------------------------------
	// 履歴へ渡す
	//
	//	履歴が繋がっていない場合でも操作だけは行う
	//	（エディタ以外からの利用や、初期化途中でも機能が壊れないようにするため）
	// -------------------------------------------------------------------------------
	bool Submit(EditorContext& _ctx, std::unique_ptr<IEditorCommand> _pCommand)
	{
		if (_pCommand == nullptr)
		{ return false; }

		if (_ctx.pHistory != nullptr)
		{
			return _ctx.pHistory->Execute(_ctx, std::move(_pCommand));
		}

		return _pCommand->Execute(_ctx);
	}
}

// -------------------------------------------------------------------------------
// Transformの読み出し
// -------------------------------------------------------------------------------
Editor::TransformValues Editor::ObjectCommands::ReadTransform(const GameObject& _object)
{
	TransformValues values;

	// GetComponentは非constのため、読み出しのためだけに一時的にconstを外す
	const auto* pTransform = const_cast<GameObject&>(_object).GetComponent<TransformComponent>();
	if (pTransform == nullptr)
	{
		return values;	// 持っていない場合は既定値
	}

	values.Position	= pTransform->GetPosition();
	values.Rotation	= pTransform->GetRotation();
	values.Scale	= pTransform->GetScale();

	return values;
}

// -------------------------------------------------------------------------------
// Transformの書き込み
// -------------------------------------------------------------------------------
void Editor::ObjectCommands::ApplyTransform(GameObject& _object, const TransformValues& _values)
{
	auto* pTransform = _object.GetComponent<TransformComponent>();
	if (pTransform == nullptr)
	{ return; }

	pTransform->SetPosition(_values.Position);
	pTransform->SetRotation(_values.Rotation);
	pTransform->SetScale   (_values.Scale);
}

// -------------------------------------------------------------------------------
// 空のオブジェクトを作る
// -------------------------------------------------------------------------------
GameObject* Editor::ObjectCommands::CreateEmpty(
	EditorContext&				_ctx,
	std::string_view			_baseName,
	const DirectX::XMFLOAT3&	_position)
{
	if (_ctx.pObjects == nullptr)
	{
		return nullptr;	// シーンが読み込まれていない
	}

	auto pObject = GameObjectFactory::CreateEmpty(*_ctx.pObjects, _baseName, _position);
	if (pObject == nullptr)
	{
		return nullptr;
	}

	// コマンドが実体を預かるため、戻り値用のポインタを先に取っておく
	GameObject* pResult = pObject.get();

	const bool submitted = Submit(_ctx, std::make_unique<ObjectLifetimeCommand>(
		ObjectLifetimeCommand::Mode::Create, std::move(pObject), nullptr, "オブジェクトの作成"));

	return submitted ? pResult : nullptr;
}

// -------------------------------------------------------------------------------
// 複製
// -------------------------------------------------------------------------------
GameObject* Editor::ObjectCommands::Duplicate(EditorContext& _ctx, const GameObject* _pSource)
{
	if (_ctx.pObjects == nullptr || _pSource == nullptr)
	{
		return nullptr;
	}

	auto pClone = GameObjectFactory::Clone(*_ctx.pObjects, *_pSource);
	if (pClone == nullptr)
	{
		return nullptr;
	}

	GameObject* pResult = pClone.get();

	const bool submitted = Submit(_ctx, std::make_unique<ObjectLifetimeCommand>(
		ObjectLifetimeCommand::Mode::Create, std::move(pClone), nullptr, "オブジェクトの複製"));

	return submitted ? pResult : nullptr;
}

// -------------------------------------------------------------------------------
// 外で組み立て済みのオブジェクトを置く
// -------------------------------------------------------------------------------
GameObject* Editor::ObjectCommands::Spawn(
	EditorContext&				_ctx,
	std::unique_ptr<GameObject>	_pObject,
	std::string_view			_label)
{
	if (_ctx.pObjects == nullptr || _pObject == nullptr)
	{
		return nullptr;
	}

	GameObject* pResult = _pObject.get();

	const bool submitted = Submit(_ctx, std::make_unique<ObjectLifetimeCommand>(
		ObjectLifetimeCommand::Mode::Create, std::move(_pObject), nullptr, std::string(_label)));

	return submitted ? pResult : nullptr;
}

// -------------------------------------------------------------------------------
// 削除
// -------------------------------------------------------------------------------
bool Editor::ObjectCommands::Destroy(EditorContext& _ctx, GameObject* _pTarget)
{
	if (!IsTargetAlive(_ctx, _pTarget))
	{
		return false;
	}

	return Submit(_ctx, std::make_unique<ObjectLifetimeCommand>(
		ObjectLifetimeCommand::Mode::Destroy, nullptr, _pTarget, "オブジェクトの削除"));
}

// -------------------------------------------------------------------------------
// 名前の変更
// -------------------------------------------------------------------------------
bool Editor::ObjectCommands::Rename(
	EditorContext&		_ctx,
	GameObject*			_pTarget,
	std::string_view	_newName)
{
	if (!IsTargetAlive(_ctx, _pTarget))
	{
		return false;
	}

	// 空の名前はヒエラルキー上で行が消えたように見えるため受け付けない
	if (_newName.empty())
	{
		return false;
	}

	// 自分自身は重複判定から外す。変更前の自分と衝突して連番が付くのを防ぐ
	const std::string newName = GameObjectFactory::MakeUniqueName(*_ctx.pObjects, _newName, _pTarget);

	// 変化が無いなら履歴を汚さない
	if (newName == _pTarget->GetName())
	{
		return false;
	}

	return Submit(_ctx, std::make_unique<RenameObjectCommand>(
		_pTarget, _pTarget->GetName(), newName));
}

// -------------------------------------------------------------------------------
// アクティブ状態の変更
// -------------------------------------------------------------------------------
bool Editor::ObjectCommands::SetActive(EditorContext& _ctx, GameObject* _pTarget, bool _active)
{
	if (!IsTargetAlive(_ctx, _pTarget))
	{
		return false;
	}

	if (_pTarget->IsActive() == _active)
	{
		return false;
	}

	return Submit(_ctx, std::make_unique<SetActiveCommand>(
		_pTarget, _pTarget->IsActive(), _active));
}

// -------------------------------------------------------------------------------
// Transformの変更
// -------------------------------------------------------------------------------
bool Editor::ObjectCommands::SetTransform(
	EditorContext&			_ctx,
	GameObject*				_pTarget,
	const TransformValues&	_before,
	const TransformValues&	_after)
{
	if (!IsTargetAlive(_ctx, _pTarget))
	{
		return false;
	}

	// 値が動いていないドラッグ（クリックしただけ等）は履歴に残さない
	const auto isSame = [](const DirectX::XMFLOAT3& _a, const DirectX::XMFLOAT3& _b)
	{
		return _a.x == _b.x && _a.y == _b.y && _a.z == _b.z;
	};

	if (isSame(_before.Position, _after.Position) &&
		isSame(_before.Rotation, _after.Rotation) &&
		isSame(_before.Scale,    _after.Scale))
	{
		return false;
	}

	return Submit(_ctx, std::make_unique<SetTransformCommand>(_pTarget, _before, _after));
}
