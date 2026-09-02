// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "SceneEditor.h"

#include <Engine/Asset/SceneAsset/SceneAsset.h>
#include <Engine/GameObject/GameObjectManager.h>
#include <Engine/RHI/Core/Device/Device.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

#include <Editor/Assets/AssetDatabase.h>
#include <Editor/Commands/CommandHistory/CommandHistory.h>

namespace
{
	// -------------------------------------------------------------------------------
	// いま開いているシーンファイル
	//
	//	エディタ全体で1つしか無い状態なので、ファイル内のstaticで持つ
	//	（同時に2つのシーンを開く仕組みが要るようになったら、
	//	  EditorAppの持ち物へ移すことになる）
	// -------------------------------------------------------------------------------
	std::filesystem::path s_CurrentPath;

	// -------------------------------------------------------------------------------
	// シーンの中身を捨てる前に、GPUの処理が終わるのを待つ
	//
	//	コマンドリストはCPUより1〜2フレーム遅れて実行される
	//	オブジェクトを消すと、そのコンポーネントが持つ定数バッファも一緒に解放されるが、
	//	GPUがまだそれを読んでいる最中だと、そこで処理が止まってしまう
	//
	//	待ちは重い処理だが、シーンを開き直す瞬間にしか起きないため負担にならない
	//	（エフェクトエディタを閉じるときと同じ考え方）
	// -------------------------------------------------------------------------------
	void WaitForGpuBeforeClear(Editor::EditorContext& _ctx)
	{
		if (_ctx.pDevice != nullptr)
		{
			_ctx.pDevice->WaitForGPU();
		}
	}

	// -------------------------------------------------------------------------------
	// 読み込み後の後始末
	//
	//	シーンを開くとオブジェクトがすべて作り直されるため、
	//	それ以前を指していた選択と履歴は無効になる
	//	どちらも必ず一緒に捨てる必要があるので、1か所にまとめている
	// -------------------------------------------------------------------------------
	void ResetEditorState(Editor::EditorContext& _ctx)
	{
		if (_ctx.pSelection != nullptr)
		{
			_ctx.pSelection->Clear();
		}

		if (_ctx.pHistory != nullptr)
		{
			_ctx.pHistory->Clear();
		}
	}
}

// -------------------------------------------------------------------------------
// 名前を付けて保存
// -------------------------------------------------------------------------------
bool Editor::SceneEditor::SaveAs(
	EditorContext&			_ctx,
	std::string_view		_name,
	std::filesystem::path&	_outPath)
{
	if (_ctx.pObjects == nullptr || _ctx.pAssets == nullptr)
	{
		return false;
	}

	// 今シーンに置いてあるものを、まるごと値へ写し取る
	const SceneAsset scene = SceneIO::Capture(*_ctx.pObjects, _name);

	// ファイルの作成そのものはAssetDatabaseの仕事
	// 保存先フォルダの決定も、重複時の連番も、すでにそちらが持っている
	const bool saved = _ctx.pAssets->CreateFile(
		_name,
		SceneIO::kExtension,
		SceneIO::Serialize(scene),
		_outPath);

	if (saved)
	{
		// 以後の「上書き保存」はこのファイルが対象になる
		s_CurrentPath = _outPath;
	}

	return saved;
}

// -------------------------------------------------------------------------------
// 上書き保存
// -------------------------------------------------------------------------------
bool Editor::SceneEditor::Save(EditorContext& _ctx)
{
	if (_ctx.pObjects == nullptr || s_CurrentPath.empty())
	{
		return false;	// 保存先がまだ決まっていない
	}

	const SceneAsset scene = SceneIO::Capture(*_ctx.pObjects, GetCurrentName());

	return SceneIO::SaveToFile(s_CurrentPath, scene);
}

// -------------------------------------------------------------------------------
// シーンを開く
// -------------------------------------------------------------------------------
bool Editor::SceneEditor::Open(EditorContext& _ctx, const std::filesystem::path& _path)
{
	if (_ctx.pObjects == nullptr)
	{
		return false;
	}

	SceneAsset scene;

	if (!SceneIO::LoadFromFile(_path, scene))
	{
		return false;
	}

	// -------------------------------------------------------------------------------
	// 中身を置き換える
	//
	// ここに来るのはUIの組み立て中なので、
	// Update / Submit のループの外。安全に作り直せる
	// -------------------------------------------------------------------------------
	// 今の中身を壊す前に、GPUが使い終わるのを待つ
	WaitForGpuBeforeClear(_ctx);

	SceneIO::Apply(*_ctx.pObjects, scene);

	ResetEditorState(_ctx);

	s_CurrentPath = _path;

	DLOG("SceneEditor : opened %s", _path.string().c_str());

	return true;
}

// -------------------------------------------------------------------------------
// 新規作成（空にする）
// -------------------------------------------------------------------------------
void Editor::SceneEditor::CreateNew(EditorContext& _ctx)
{
	if (_ctx.pObjects == nullptr)
	{
		return;
	}

	WaitForGpuBeforeClear(_ctx);

	_ctx.pObjects->Clear();

	ResetEditorState(_ctx);

	// 保存先の記憶も消す。次の保存は「名前を付けて保存」になる
	s_CurrentPath.clear();
}

// -------------------------------------------------------------------------------
// いま開いているシーン
// -------------------------------------------------------------------------------
const std::filesystem::path& Editor::SceneEditor::GetCurrentPath()
{
	return s_CurrentPath;
}

std::string Editor::SceneEditor::GetCurrentName()
{
	if (s_CurrentPath.empty())
	{
		return "New Scene";
	}

	return s_CurrentPath.stem().string();
}

bool Editor::SceneEditor::HasCurrentPath()
{
	return !s_CurrentPath.empty();
}
