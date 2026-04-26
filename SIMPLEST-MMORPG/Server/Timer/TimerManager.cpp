#include "TimerManager.h"
#include <iostream>

TimerManager& TimerManager::GetInstance()
{
	static TimerManager instance;
	return instance;
}

void TimerManager::Start(HANDLE hIOCP)
{
	m_hIOCP = hIOCP;
	m_running = true;
	m_thread = std::thread(&TimerManager::WorkerThread, this);
}

void TimerManager::Stop()
{
	if (!m_running)
	{
		return;
	}

	m_running = false;
	m_cv.notify_all();

	if (m_thread.joinable())
	{
		m_thread.join();
	}
}

void TimerManager::AddTimer(TimerType type, uint32_t targetId, int delayMs)
{
	auto fireTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);
	
	{
		std::lock_guard lock(m_mutex);
		m_queue.push({ fireTime, type, targetId });
	}

	m_cv.notify_one(); // 타이머 스레드 깨우기
}

void TimerManager::WorkerThread()
{
	while (m_running)
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		// 큐가 비어있으면 새 이벤트까지 대기
		if (m_queue.empty())
		{
			m_cv.wait(lock, [this] { return !m_queue.empty() || !m_running; });
			if (!m_running)
			{
				break;
			}
		}

		TimerEvent next = m_queue.top();
		auto now = std::chrono::steady_clock::now();

		if (next.fireTime > now)
		{
			m_cv.wait_until(lock, next.fireTime);
			continue;
		}

		m_queue.pop();
		lock.unlock();

		PostToIOCP(next);
	}
}

void TimerManager::PostToIOCP(const TimerEvent& ev)
{
	TimerOverlapped* tov = m_pool.Acquire();
	ZeroMemory(&tov->base.overlapped, sizeof(WSAOVERLAPPED));
	tov->base.ioType = IOType::TIMER;
	tov->type = ev.type;
	tov->targetId = ev.targetId;

	PostQueuedCompletionStatus(m_hIOCP, 0, 0, &tov->base.overlapped);
}
