#include "StringUtil.h"

namespace StringUtil
{
	std::string ToUTF8(const std::wstring& _value)
	{
		const int len = WideCharToMultiByte(
			CP_UTF8, 0, _value.data(), -1,
			nullptr, 0, nullptr, nullptr);

		std::string result(len, '\0');
		WideCharToMultiByte(
			CP_UTF8, 0, _value.data(), -1,
			result.data(), len, nullptr, nullptr);

		return result;

	}

	std::wstring ToWString(const char* _value)
	{
		const int len = MultiByteToWideChar(
			CP_UTF8, 0, _value, -1, nullptr, 0);

		std::wstring result(len, L'\0');
		MultiByteToWideChar(
			CP_UTF8, 0, _value, -1, result.data(), len);

		return result;
	}

	std::wstring ToWString(const std::string& _value)
	{
		const int len = MultiByteToWideChar(
			CP_UTF8, 0, _value.data(), -1, nullptr, 0);

		std::wstring result(len, L'\0');
		MultiByteToWideChar(
			CP_UTF8, 0, _value.data(), -1, result.data(), len);

		return result;
	}

	std::wstring Utf8ToWide(std::string_view _utf8)
	{
		if (_utf8.empty())
		{ return{}; }

		const int srcLength = static_cast<int>(_utf8.size());

		// 第4引数に明示的な長さを渡しているため、戻り値にヌル終端は含まれない
		const int len = MultiByteToWideChar(CP_UTF8, 0, _utf8.data(), srcLength, nullptr, 0);
		if (len <= 0)
		{ return{}; }

		std::wstring result(static_cast<size_t>(len), L'\0');

		// 出力バッファのサイズには len をそのまま渡す
		// ここに 0 を渡すと「必要サイズの問い合わせ」と解釈され、1文字も書き込まれない
		MultiByteToWideChar(CP_UTF8, 0, _utf8.data(), srcLength, result.data(), len);
		return result;
	}

	std::string WideToUtf8(std::wstring_view _wide)
	{
		if (_wide.empty())
		{ return{}; }

		const int srcLength = static_cast<int>(_wide.size());

		const int len = WideCharToMultiByte(
			CP_UTF8, 0, _wide.data(), srcLength,
			nullptr, 0, nullptr, nullptr);

		if (len <= 0)
		{ return{}; }

		std::string result(static_cast<size_t>(len), '\0');
		WideCharToMultiByte(
			CP_UTF8, 0, _wide.data(), srcLength,
			result.data(), len, nullptr, nullptr);

		return result;
	}
}
