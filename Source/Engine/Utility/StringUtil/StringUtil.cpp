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
}