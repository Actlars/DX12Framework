#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Editor/EditorContext.h>

class GameObject;

namespace Editor
{
	struct ComponentTypeInfo;

	// -------------------------------------------------------------------------------
	// ComponentCommands
	//
	// 概要 :
	//	画面から呼ぶ、コンポーネントの付け外し（取り消し可能）
	//
	//	「外す」を取り消すときは、外す直前の値まで戻す
	//	付け直すだけでは、位置や表示状態といった設定が失われてしまうため、
	//	削除前に ComponentRegistry で値を写し取ってから外している
	//
	// 責務の分担 :
	//	ComponentRegistry	型ごとの付け外し・値の写し取りの手順
	//	ComponentCommands	1回分の操作としてのまとめ上げと履歴への登録（このファイル）
	//	InspectorPanel		どこにボタンを置くか
	// -------------------------------------------------------------------------------
	namespace ComponentCommands
	{
		// -------------------------------------------------------------------------------
		// @brief	コンポーネントを付ける
		//
		// @param[in]	_pTarget	付ける先
		// @param[in]	_typeInfo	付ける種別（ComponentRegistryの表から取る）
		// @return	true : 付けた。すでに付いている場合はfalse
		// -------------------------------------------------------------------------------
		bool Add(
			EditorContext&				_ctx,
			GameObject*					_pTarget,
			const ComponentTypeInfo&	_typeInfo);

		// -------------------------------------------------------------------------------
		// @brief	コンポーネントを外す
		//
		//	外せない種別（Transformなど）を指定した場合は何もしない
		//
		// @return	true : 外した
		// -------------------------------------------------------------------------------
		bool Remove(
			EditorContext&				_ctx,
			GameObject*					_pTarget,
			const ComponentTypeInfo&	_typeInfo);
	}
}
