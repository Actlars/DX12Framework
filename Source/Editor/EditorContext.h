#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Context/Context.h>
#include <Editor/Selection/Selection.h>

namespace EditorUI { class Font; }

class GameObjectManager;
class ViewportTarget;
class SceneManager;

namespace Editor
{
	class AssetDatabase;
	class PanelManager;

	// -------------------------------------------------------------------------------
	// EditorContext struct
	//
	// 概要 :
	//	すべてのパネルが共有する「参照の束」
	//
	//	パネル同士が直接知り合わないようにするための仕組み
	//	例えばコンテンツブラウザがエフェクトエディタを開きたい場合、
	//	相手のクラスではなくPanelManager越しに依頼する
	//	これにより、パネルは1枚ずつ独立して追加・削除できる
	//
	//	所有権はすべてEditorAppが持ち、ここにあるのは参照だけ
	// -------------------------------------------------------------------------------
	struct EditorContext
	{
		EditorUI::Context*	pUI		= nullptr;	// ウィジェットの発行先
		EditorUI::Font*		pFont	= nullptr;	// 文字の描画に使うフォント

		PanelManager*		pPanels		= nullptr;	// パネルの開閉と生成
		Selection*			pSelection	= nullptr;	// 選択状態の共有
		AssetDatabase*		pAssets		= nullptr;	// コンテンツブラウザのファイル木

		GameObjectManager*	pObjects	= nullptr;	// 現在のシーンの中身(無い場合はnullptr)
		SceneManager*		pScenes		= nullptr;	// シーンの問い合わせ
		ViewportTarget*		pViewport	= nullptr;	// ゲーム画面のレンダーターゲット

		float DeltaTime = 0.0f;	// 前フレームからの経過時間(秒)

		// パネルが「使える状態か」を判定するための小さなヘルパー
		bool IsValid() const { return pUI != nullptr && pFont != nullptr; }
	};
}
