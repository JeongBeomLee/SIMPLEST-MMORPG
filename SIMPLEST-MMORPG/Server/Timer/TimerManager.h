#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <cstdint>
#include "../Network/NetworkTypes.h"

struct TimerEvent
{
	std::chrono::steady_clock::time_point fireTime;
	TimerType type;
	uint32_t targetId;

	bool operator>(const TimerEvent& other) const
	{
		return fireTime > other.fireTime;
	}
};

class TimerManager
{
public:
	static TimerManager& GetInstance();

	TimerManager(const TimerManager&) = delete;
	TimerManager& operator=(const TimerManager&) = delete;

	// IOCP 핸들 받아서 타이머 스레드 시작
	void Start(HANDLE hIOCP);

	// 스레드 종료
	void Stop();

	// 타이머 등록 (지연 시간 후 발사)
	void AddTimer(TimerType type, uint32_t targetId, int delayMs);

private:
	TimerManager() = default;
	~TimerManager() = default;

	void ThreadLoop();
	void PostToIOCP(const TimerEvent& ev);

private:
	HANDLE m_hIOCP{ NULL };
	std::atomic<bool> m_running{ false };
	std::thread m_thread;

	std::priority_queue<TimerEvent, std::vector<TimerEvent>, std::greater<TimerEvent>> m_queue;
	std::mutex m_mutex;
	std::condition_variable m_cv;
};

