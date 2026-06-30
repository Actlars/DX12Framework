// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "FileUtil.h"

// -------------------------------------------------------------------------------
// ファイルパスを検索
// -------------------------------------------------------------------------------
bool SearchFilePath(const wchar_t* _filename, std::wstring& _result)
{
	if (_filename == nullptr)
	{
		return false;
	}

	if (wcscmp(_filename, L" ") == 0 || wcscmp(_filename, L"") == 0)
	{
		return false;
	}

	wchar_t exePath[520] = {};
	GetModuleFileNameW(nullptr, exePath, 520);
	exePath[519] = L'\0';	// null終端化
	PathRemoveFileSpecW(exePath);

	wchar_t dstPath[520] = {};

	wcscpy_s(dstPath, _filename);
	if (PathFileExistsW(dstPath) == TRUE)
	{
		_result = dstPath;
		return true;
	}

	swprintf_s(dstPath, L"..\\%s", _filename);
	if (PathFileExistsW(dstPath) == TRUE)
	{
		_result = dstPath;
		return true;
	}

	swprintf_s(dstPath, L"..\\..\\%s", _filename);
	if (PathFileExistsW(dstPath) == TRUE)
	{
		_result = dstPath;
		return true;
	}

	swprintf_s(dstPath, L"\\res\\%s", _filename);
	if (PathFileExistsW(dstPath) == TRUE)
	{
		_result = dstPath;
		return true;
	}

	swprintf_s(dstPath, L"%s\\%s", exePath, _filename);
	if (PathFileExistsW(dstPath) == TRUE)
	{
		_result = dstPath;
		return true;
	}

	swprintf_s(dstPath, L"%s\\..\\%s", exePath, _filename);
	if (PathFileExistsW(dstPath) == TRUE)
	{
		_result = dstPath;
		return true;
	}

	swprintf_s(dstPath, L"%s\\..\\%s", exePath, _filename);
	if (PathFileExistsW(dstPath) == TRUE)
	{
		_result = dstPath;
		return true;
	}

	swprintf_s(dstPath, L"%s\\..\\..\\%s", exePath, _filename);
	if (PathFileExistsW(dstPath) == TRUE)
	{
		_result = dstPath;
		return true;
	}

	swprintf_s(dstPath, L"%s\\res\\%s", exePath, _filename);
	if (PathFileExistsW(dstPath) == TRUE)
	{
		_result = dstPath;
		return true;
	}

	return false;
}

