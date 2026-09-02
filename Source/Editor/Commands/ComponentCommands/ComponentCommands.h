#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Editor/EditorContext.h>

class GameObject;
struct ComponentTypeInfo;

namespace Editor
{
	// -------------------------------------------------------------------------------
	// ComponentCommands
	//
	// 概要 :
	//	画面から呼ぶ、コンポーネントに対する取り消し可能な操作
	//
	//	「外す」を取り消すときは、外す直前の値まで戻す
	//	付け直すだけでは、位置やモデルの指定といった設定が失われてしまうため、
	//	削除前に ComponentRegistry で値を写し取ってから外している
	//
	// 責務の分担 :
	//	ComponentRegistry	型ごとの付け外し・値の写し取りの手順（エンジン）
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
		//	外せない種別を指定した場合でも壊れないよう、呼び出し側で判断する
		//
		// @return	true : 外した
		// -------------------------------------------------------------------------------
		bool Remove(
			EditorContext&				_ctx,
			GameObject*					_pTarget,
			const ComponentTypeInfo&	_typeInfo);

		// -------------------------------------------------------------------------------
		// @brief	コンポーネントの値の変更を、取り消せる操作として記録する
		//
		//	値そのものはすでに書き換わっている前提で、
		//	「書き換える前の値」と「書き換えたあとの値」を渡す
		//
		//	型ごとに専用のコマンドを作らないための仕組み
		//	ComponentRegistry が持つ 保存 / 復元 をそのまま使うため、
		//	保存できる値であれば何を変えてもUndoできる
		//
		// @param[in]	_typeInfo	対象の種別
		// @param[in]	_before		書き換える前の値（CaptureComponentで写したもの）
		// @param[in]	_after		書き換えたあとの値
		// @return	true : 履歴へ積んだ。値に変化が無ければfalse
		// -------------------------------------------------------------------------------
		bool SetValues(
			EditorContext&				_ctx,
			GameObject*					_pTarget,
			const ComponentTypeInfo&	_typeInfo,
			const nlohmann::json&		_before,
			const nlohmann::json&		_after);
	}
}
