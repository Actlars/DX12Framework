// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "ComponentCommands.h"

#include <Engine/GameObject/GameObject.h>

#include <Editor/Commands/CommandHistory/CommandHistory.h>
#include <Editor/Components/ComponentRegistry/ComponentRegistry.h>
#include <Editor/Scene/GameObjectFactory/GameObjectFactory.h>

namespace
{
	using namespace Editor;

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
	//	取り消したときに、位置や表示状態まで含めて元通りにするため
	//	（付け直すだけでは既定値のコンポーネントになってしまう）
	// -------------------------------------------------------------------------------
	class ComponentCommand : public IEditorCommand
	{
	public:

		enum class Mode
		{
			Add,		// 実行で付ける
			Remove,		// 実行で外す
		};

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

		bool Execute(EditorContext& _ctx) override
		{
			return (m_Mode == Mode::Add) ? Attach(_ctx) : Detach(_ctx);
		}

		void Undo(EditorContext& _ctx) override
		{
			if (m_Mode == Mode::Add)	{ Detach(_ctx); }
			else						{ Attach(_ctx); }
		}

		std::string_view GetLabel() const override { return m_Label; }

	private:

		// 付ける（外す前の値を持っていれば、それも戻す）
		bool Attach(EditorContext& _ctx)
		{
			if (!GameObjectFactory::IsAlive(_ctx, m_pObject))
			{ return false; }

			ComponentRegistry::RestoreComponent(*m_pObject, *m_pTypeInfo, m_State);
			return true;
		}

		// 外す
		bool Detach(EditorContext& _ctx)
		{
			if (!GameObjectFactory::IsAlive(_ctx, m_pObject))
			{ return false; }

			if (m_pTypeInfo->Remove == nullptr)
			{ return false; }

			return m_pTypeInfo->Remove(*m_pObject);
		}

		Mode						m_Mode;
		GameObject*					m_pObject;
		const ComponentTypeInfo*	m_pTypeInfo;	// 表の実体は起動中ずっと存在する
		nlohmann::json				m_State;		// 外す直前の値
		std::string					m_Label;
	};

	// 履歴へ渡す（履歴が無い場合でも操作だけは行う）
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
// コンポーネントを付ける
// -------------------------------------------------------------------------------
bool Editor::ComponentCommands::Add(
	EditorContext&				_ctx,
	GameObject*					_pTarget,
	const ComponentTypeInfo&	_typeInfo)
{
	if (!GameObjectFactory::IsAlive(_ctx, _pTarget))
	{
		return false;
	}

	// すでに付いている場合は何もしない
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
		std::string(_typeInfo.DisplayName) + " の追加"));
}

// -------------------------------------------------------------------------------
// コンポーネントを外す
// -------------------------------------------------------------------------------
bool Editor::ComponentCommands::Remove(
	EditorContext&				_ctx,
	GameObject*					_pTarget,
	const ComponentTypeInfo&	_typeInfo)
{
	if (!GameObjectFactory::IsAlive(_ctx, _pTarget))
	{
		return false;
	}

	// 外せない種別（Transformなど）は画面側でも選べないようにしているが、
	// 呼ばれても壊れないようにここでも確認する
	if (!_typeInfo.IsRemovable)
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
		std::string(_typeInfo.DisplayName) + " の削除"));
}
