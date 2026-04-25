#pragma once
#include <Windows.h>
#include <sstream>
#include <fstream>
#include <mutex>

namespace LogInternal
{
	inline std::ofstream& GetLogFile()
	{
		// std::ios::trunc - 매 실행마다 파일 초기화 (무한 증가 방지)
		static std::ofstream file("client.log", std::ios::trunc);
		return file;
	}

	inline std::mutex& GetLogMutex()
	{
		static std::mutex mtx;
		return mtx;
	}

	inline std::wstring AsciiToWString(const std::string& s)
	{
		std::wstring result;
		result.reserve(s.size());
		for (char c : s) result.push_back(static_cast<wchar_t>(c));
		return result;
	}
}

#ifdef _DEBUG
#define LOG(msg) do { \
        std::ostringstream _oss; \
        _oss << msg << "\n"; \
        std::string _str = _oss.str(); \
        OutputDebugStringA(_str.c_str()); \
        std::lock_guard _lock(LogInternal::GetLogMutex()); \
        LogInternal::GetLogFile() << _str; \
        LogInternal::GetLogFile().flush(); \
    } while(0)
#else
#define LOG(msg) do { \
        std::ostringstream _oss; \
        _oss << msg << "\n"; \
        std::string _str = _oss.str(); \
        std::lock_guard _lock(LogInternal::GetLogMutex()); \
        LogInternal::GetLogFile() << _str; \
        LogInternal::GetLogFile().flush(); \
    } while(0)
#endif