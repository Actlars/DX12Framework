#pragma once

namespace Editor
{
	// -------------------------------------------------------------------------------
	// DirectoryWatcher class
	//
	// 概要 :
	//	フォルダの中身が外部から変更されたことを検知するクラス
	//
	//	エクスプローラーでファイルを追加・削除・リネームしても、
	//	エディタ側が気づかなければ表示は古いままになる
	//	このクラスは「変わったかどうか」だけを教え、
	//	何をどう読み直すかは呼び出し側（AssetDatabase）に任せる
	//
	// 実装方針 :
	//	FindFirstChangeNotificationによるOSの通知を使う
	//	待機はタイムアウト0で行うため、専用スレッドを持たずに
	//	毎フレームPoll()を呼ぶだけで済む
	//
	//	ポーリングでフォルダを走査する方式に比べ、
	//	変化がないときのコストがほぼゼロになる
	// -------------------------------------------------------------------------------
	class DirectoryWatcher
	{
	public:

		DirectoryWatcher() = default;
		~DirectoryWatcher();

		// -------------------------------------------------------------------------------
		// @brief	監視を開始する
		//
		//	すでに監視中の場合は、いったん停止してから張り直す
		//
		// @param[in]	_directory		監視するフォルダ
		// @param[in]	_watchSubtree	true なら配下のフォルダもまとめて監視する
		// @retval	true	監視を開始できた
		// -------------------------------------------------------------------------------
		bool Start(const std::filesystem::path& _directory, bool _watchSubtree = true);

		void Stop();

		// -------------------------------------------------------------------------------
		// @brief	前回の確認以降に変化があったかを返す
		//
		//	待たずに即座に戻るため、毎フレーム呼んで構わない
		//	true を返した時点で次回分の監視を張り直すので、
		//	呼び出し側は「true なら読み直す」とだけ書けばよい
		// -------------------------------------------------------------------------------
		bool Poll();

		bool IsWatching() const { return m_Handle != nullptr; }

		const std::filesystem::path& GetDirectory() const { return m_Directory; }

	private:

		// INVALID_HANDLE_VALUE と nullptr を混ぜないよう、内部では nullptr を「未監視」として扱う
		HANDLE					m_Handle = nullptr;
		std::filesystem::path	m_Directory;

		DirectoryWatcher	(const DirectoryWatcher&) = delete;
		void operator =		(const DirectoryWatcher&) = delete;
	};
}
