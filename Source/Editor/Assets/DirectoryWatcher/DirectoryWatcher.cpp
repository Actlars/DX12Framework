// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "DirectoryWatcher.h"
#include <Engine/Utility/Debug/Logger/Logger.h>

namespace
{
	// -------------------------------------------------------------------------------
	// 監視する変更の種類
	//
	// ファイル名・フォルダ名の増減とリネーム、そして書き込みの完了を拾う
	// 属性や最終アクセス時刻まで含めると、開いただけで通知が飛んで無駄が多くなる
	// -------------------------------------------------------------------------------
	constexpr DWORD kNotifyFilter =
		FILE_NOTIFY_CHANGE_FILE_NAME |
		FILE_NOTIFY_CHANGE_DIR_NAME  |
		FILE_NOTIFY_CHANGE_LAST_WRITE;
}

Editor::DirectoryWatcher::~DirectoryWatcher()
{
	Stop();
}

// -------------------------------------------------------------------------------
// 監視の開始
// -------------------------------------------------------------------------------
bool Editor::DirectoryWatcher::Start(const std::filesystem::path& _directory, bool _watchSubtree)
{
	Stop();

	std::error_code error;
	if (!std::filesystem::is_directory(_directory, error))
	{
		return false;
	}

	HANDLE handle = FindFirstChangeNotificationW(
		_directory.c_str(),
		_watchSubtree ? TRUE : FALSE,
		kNotifyFilter);

	if (handle == INVALID_HANDLE_VALUE)
	{
		ELOG("DirectoryWatcher::Start() FindFirstChangeNotification failed");
		return false;
	}

	m_Handle	= handle;
	m_Directory	= _directory;
	return true;
}

// -------------------------------------------------------------------------------
// 監視の停止
// -------------------------------------------------------------------------------
void Editor::DirectoryWatcher::Stop()
{
	if (m_Handle != nullptr)
	{
		FindCloseChangeNotification(m_Handle);
		m_Handle = nullptr;
	}

	m_Directory.clear();
}

// -------------------------------------------------------------------------------
// 変化の確認
//
// タイムアウト0で問い合わせるだけなので、フレームを止めることはない
// -------------------------------------------------------------------------------
bool Editor::DirectoryWatcher::Poll()
{
	if (m_Handle == nullptr)
	{
		return false;
	}

	bool changed = false;

	// -------------------------------------------------------------------------------
	// 短時間に複数の変更が起きると通知が連続して溜まる
	// まとめて吸い出し、呼び出し側の読み直しは1回で済むようにする
	// -------------------------------------------------------------------------------
	while (WaitForSingleObject(m_Handle, 0) == WAIT_OBJECT_0)
	{
		changed = true;

		// 次の変更を受け取れるよう、監視を張り直す
		if (!FindNextChangeNotification(m_Handle))
		{
			// 監視対象そのものが消えた場合など。以降は通知を受け取れない
			ELOG("DirectoryWatcher::Poll() FindNextChangeNotification failed");
			Stop();
			break;
		}
	}

	return changed;
}
