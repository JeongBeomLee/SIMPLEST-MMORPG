#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace LogInternal
{
	inline std::ofstream& GetFile()
	{
		static std::ofstream file("server.log", std::ios::trunc);
		return file;
	}

	inline std::mutex& GetMutex()
	{
		static std::mutex mtx;
		return mtx;
	}

	inline std::string Timestamp()
	{
		auto now = std::chrono::system_clock::now();
		auto t = std::chrono::system_clock::to_time_t(now);
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
		std::tm tm;
		localtime_s(&tm, &t);
		std::ostringstream oss;
		oss << std::put_time(&tm, "%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms.count();
		return oss.str();
	}
}

#define LOG_AT(level, msg) do { \
		std::ostringstream _oss; \
		_oss << '[' << LogInternal::Timestamp() << "][" << level << "] " << msg << '\n'; \
		std::string _s = _oss.str(); \
		std::lock_guard _lock(LogInternal::GetMutex()); \
		std::cout << _s; \
		LogInternal::GetFile() << _s; \
		LogInternal::GetFile().flush(); \
	} while(0)

#define LOG_INFO(msg)  LOG_AT("INFO ", msg)
#define LOG_WARN(msg)  LOG_AT("WARN ", msg)
#define LOG_ERROR(msg) LOG_AT("ERROR", msg)

#ifdef _DEBUG
#define LOG_DEBUG(msg) LOG_AT("DEBUG", msg)
#else
#define LOG_DEBUG(msg) ((void)0)
#endif