#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Context/Context.h>
#include <Editor/Selection/Selection.h>

namespace EditorUI { class Font; }

class GameObjectManager;
class ModelLibrary;
class ViewportTarget;
class SceneManager;
class EditorUIRenderer;

namespace RHI { class Device; }

namespace Editor
{
	class AssetDatabase;
	class PanelManager;
	class CommandHistory;

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

		// -------------------------------------------------------------------------------
		// 操作の履歴（Undo / Redo）
		//
		//	シーンを書き換える操作は、どのパネルから行っても必ずここを通す
		//	そうしないと「一部の操作だけ取り消せない」という分かりにくい状態になる
		//	実際の呼び出しは ObjectCommands / ComponentCommands 経由で行う
		// -------------------------------------------------------------------------------
		CommandHistory*		pHistory	= nullptr;

		GameObjectManager*	pObjects	= nullptr;	// 現在のシーンの中身(無い場合はnullptr)

		// -------------------------------------------------------------------------------
		// モデルの置き場
		//
		//	インスペクタからモデルを選ぶために使う
		//	所有者はApplicationで、シーンより長生きする
		// -------------------------------------------------------------------------------
		ModelLibrary*		pModels		= nullptr;
		SceneManager*		pScenes		= nullptr;	// シーンの問い合わせ
		ViewportTarget*		pViewport	= nullptr;	// ゲーム画面のレンダーターゲット

		// -------------------------------------------------------------------------------
		// 自前のGPUリソースを持つパネル用
		//
		//	エフェクトエディタが、プレビュー専用のレンダーターゲットと
		//	GPUパーティクルを自分で確保するために必要になる
		// -------------------------------------------------------------------------------
		RHI::Device*		pDevice		= nullptr;
		EditorUIRenderer*	pUIRenderer	= nullptr;

		float DeltaTime = 0.0f;	// 前フレームからの経過時間(秒)

		// パネルが「使える状態か」を判定するための小さなヘルパー
		bool IsValid() const { return pUI != nullptr && pFont != nullptr; }
	};
}
