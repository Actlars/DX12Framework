// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "ComponentCommands.h"

#include <Engine/GameObject/GameObject.h>
#include <Engine/GameObject/GameObjectManager.h>
#include <Engine/GameObject/GameObjectFactory/GameObjectFactory.h>
#include <Engine/GameObject/ComponentRegistry/ComponentRegistry.h>

#include <Editor/Commands/CommandHistory/CommandHistory.h>
#include <Editor/Components/ComponentInspectors/ComponentInspectors.h>

namespace
{
	using namespace Editor;

	// -------------------------------------------------------------------------------
	// 対象がまだシーンに居るか
	//
	// 履歴は生ポインタを持つため、触る前に必ず確かめる
	// -------------------------------------------------------------------------------
	bool IsTargetAlive(const EditorContext& _ctx, const GameObject* _pObject)
	{
		return _ctx.pObjects != nullptr
			&& GameObjectFactory::IsAlive(*_ctx.pObjects, _pObject);
	}

	// -------------------------------------------------------------------------------
	// ComponentCommand class
	//
	// 概要 :
	//	コンポーネントを「付ける」「外す」操作
	//
	//	付けると外すも向き違いの同じ操作なので、ObjectLifetimeCommandと同じく
	//	1つのクラスに向きだけを持たせている
	//
	//	外す前の値は m_State に写しておく
	//	取り消したときに、位置やモデルの指定まで含めて元通りにするため
	//	（付け直すだけでは既定値のコンポーネントになってしまう）
	// -------------------------------------------------------------------------------
	class ComponentCommand : public IEditorCommand
	{
	public:

		// Commandの実行方向
		enum class Mode
		{
			Add,		// ExecuteでComponentを追加
			Remove,		// ExecuteでComponentを削除
		};

		// Command実行に必要な情報を生成時にすべて保持しておく
		ComponentCommand(
			Mode						_mode,
			GameObject*					_pTarget,
			const ComponentTypeInfo&	_typeInfo,
			nlohmann::json				_state,
			std::string					_label)
			: m_Mode		(_mode)
			, m_pObject		(_pTarget)
			, m_pTypeInfo	(&_typeInfo)
			, m_State		(std::move(_state))
			, m_Label		(std::move(_label))
		{ /* DO_NOTHING */ }

		// Modeによって追加か削除を実行する
		bool Execute(EditorContext& _ctx) override
		{
			return (m_Mode == Mode::Add) ? Attach(_ctx) : Detach(_ctx);
		}

		// Executeとは逆方向の操作を行う
		void Undo(EditorContext& _ctx) override
		{
			if (m_Mode == Mode::Add)	{ Detach(_ctx); }
			else						{ Attach(_ctx); }
		}

		std::string_view GetLabel() const override { return m_Label; }

	private:

		// Componentを追加/復元（外す前の値を持っていれば、それも戻す）
		bool Attach(EditorContext& _ctx)
		{
			if (!IsTargetAlive(_ctx, m_pObject))
			{ return false; }

			// 具体的なComponent生成・状態復元はComponentRegistryへ移譲する
			ComponentRegistry::RestoreComponent(*m_pObject, *m_pTypeInfo, m_State);
			return true;
		}

		// Componentを削除
		bool Detach(EditorContext& _ctx)
		{
			if (!IsTargetAlive(_ctx, m_pObject))
			{ return false; }

			// ComponentTypeInfoに削除処理が登録されていない種類は削除できない
			if (m_pTypeInfo->Remove == nullptr)
			{ return false; }

			return m_pTypeInfo->Remove(*m_pObject);
		}

		Mode						m_Mode;			// Execute時に追加するか削除するか
		GameObject*					m_pObject;		// 操作対象GameObjectの非所有参照
		const ComponentTypeInfo*	m_pTypeInfo;	// 表の実体は起動中ずっと存在する
		nlohmann::json				m_State;		// 外す直前の値
		std::string					m_Label;		// メニュー等に表示するCommand名
	};

	// -------------------------------------------------------------------------------
	// SetComponentValuesCommand class
	//
	// 概要 :
	//	コンポーネントの値を書き換える操作
	//
	//	値の中身には一切触れず、ComponentRegistryの 保存 / 復元 に任せる
	//	そのため、モデルの指定でも表示フラグでも、
	//	保存できる値であれば同じ1つのコマンドで取り消せる
	//	（型ごとに専用のコマンドを増やさないための仕組み）
	// -------------------------------------------------------------------------------
	class SetComponentValuesCommand : public IEditorCommand
	{
	public:

		SetComponentValuesCommand(
			GameObject*					_pTarget,
			const ComponentTypeInfo&	_typeInfo,
			nlohmann::json				_before,
			nlohmann::json				_after,
			std::string					_label)
			: m_pObject		(_pTarget)
			, m_pTypeInfo	(&_typeInfo)
			, m_Before		(std::move(_before))
			, m_After		(std::move(_after))
			, m_Label		(std::move(_label))
		{ /* DO_NOTHING */ }

		bool Execute(EditorContext& _ctx) override { return Apply(_ctx, m_After); }
		void Undo	(EditorContext& _ctx) override { Apply(_ctx, m_Before); }

		std::string_view GetLabel() const override { return m_Label; }

	private:

		bool Apply(EditorContext& _ctx, const nlohmann::json& _values)
		{
			if (!IsTargetAlive(_ctx, m_pObject))
			{ return false; }

			if (m_pTypeInfo->Load == nullptr)
			{ return false; }

			// 付いていない場合もあるため、Addを兼ねたRestoreを使う
			ComponentRegistry::RestoreComponent(*m_pObject, *m_pTypeInfo, _values);
			return true;
		}

		GameObject*					m_pObject;
		const ComponentTypeInfo*	m_pTypeInfo;
		nlohmann::json				m_Before;
		nlohmann::json				m_After;
		std::string					m_Label;
	};

	// Commandを実行する（履歴が無い場合でも操作だけは行う）
	bool Submit(EditorContext& _ctx, std::unique_ptr<IEditorCommand> _pCommand)
	{
		if (_pCommand == nullptr)
		{ return false; }

		// CommandHistoryが存在する場合は、Commandの所有権をHistoryへ移す
		if (_ctx.pHistory != nullptr)
		{
			return _ctx.pHistory->Execute(_ctx, std::move(_pCommand));
		}

		// Historyを使用しない環境では、Commandを履歴へ残さずその場で1回だけ実行する
		return _pCommand->Execute(_ctx);
	}
}

// -------------------------------------------------------------------------------
// GameObjectへComponentを追加
//
// 追加に成功した場合はCommandHistoryへ履歴を残し、
// Undoによって追加前の状態に戻せるようにする
// -------------------------------------------------------------------------------
bool Editor::ComponentCommands::Add(
	EditorContext&				_ctx,
	GameObject*					_pTarget,
	const ComponentTypeInfo&	_typeInfo)
{
	if (!IsTargetAlive(_ctx, _pTarget))
	{
		return false;
	}

	// 同じComponentがすでについている場合は何もしない
	// AddComponentは既存のものを返す仕様のため、放置すると
	// 「押しても何も起きないのに履歴だけ増える」状態になる
	if (_typeInfo.Has != nullptr && _typeInfo.Has(*_pTarget))
	{
		return false;
	}

	// 付ける操作に「外す前の値」は無いため、空の状態で構わない
	return Submit(_ctx, std::make_unique<ComponentCommand>(
		ComponentCommand::Mode::Add,
		_pTarget,
		_typeInfo,
		nlohmann::json::object(),
		std::string(ComponentInspectors::GetDisplay(_typeInfo.TypeName).DisplayName) + " の追加"));
}

// -------------------------------------------------------------------------------
// GameObjectからComponentを削除
// -------------------------------------------------------------------------------
bool Editor::ComponentCommands::Remove(
	EditorContext&				_ctx,
	GameObject*					_pTarget,
	const ComponentTypeInfo&	_typeInfo)
{
	if (!IsTargetAlive(_ctx, _pTarget))
	{
		return false;
	}

	if (_typeInfo.Has == nullptr || !_typeInfo.Has(*_pTarget))
	{
		return false;	// 付いていない
	}

	// -------------------------------------------------------------------------------
	// 外す前に、今の値を写し取っておく
	// これが「元に戻す」で復元される中身になる
	// -------------------------------------------------------------------------------
	nlohmann::json state = ComponentRegistry::CaptureComponent(*_pTarget, _typeInfo);

	return Submit(_ctx, std::make_unique<ComponentCommand>(
		ComponentCommand::Mode::Remove,
		_pTarget,
		_typeInfo,
		std::move(state),
		std::string(ComponentInspectors::GetDisplay(_typeInfo.TypeName).DisplayName) + " の削除"));
}

// -------------------------------------------------------------------------------
// コンポーネントの値の変更を記録する
// -------------------------------------------------------------------------------
bool Editor::ComponentCommands::SetValues(
	EditorContext&				_ctx,
	GameObject*					_pTarget,
	const ComponentTypeInfo&	_typeInfo,
	const nlohmann::json&		_before,
	const nlohmann::json&		_after)
{
	if (!IsTargetAlive(_ctx, _pTarget))
	{
		return false;
	}

	// 値が動いていないなら履歴を汚さない
	if (_before == _after)
	{
		return false;
	}

	return Submit(_ctx, std::make_unique<SetComponentValuesCommand>(
		_pTarget,
		_typeInfo,
		_before,
		_after,
		std::string(ComponentInspectors::GetDisplay(_typeInfo.TypeName).DisplayName) + " の変更"));
}
