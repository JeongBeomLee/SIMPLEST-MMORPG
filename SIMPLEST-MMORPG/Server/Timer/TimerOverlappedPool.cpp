#include "TimerOverlappedPool.h"
#include "../Logger.h"
#include <iostream>

TimerOverlappedPool::TimerOverlappedPool(size_t capacity)
{
	m_storage.resize(capacity);

	m_rangeStart = &m_storage.front();
	m_rangeEnd = &m_storage.back() + 1;

	for (auto& tov : m_storage)
	{
		m_free.push(&tov);
	}
}

TimerOverlapped* TimerOverlappedPool::Acquire()
{
	std::lock_guard lock(m_mutex);

	if (m_free.empty())
	{
		LOG_WARN("[TimerPool] Exhausted, fallback to new()");
		return new TimerOverlapped();
	}

	TimerOverlapped* p = m_free.front();
	m_free.pop();
	return p;
}

void TimerOverlappedPool::Release(TimerOverlapped* p)
{
	if (!p)
	{
		return;
	}

	if (!IsFromPool(p))
	{
		delete p;
		return;
	}

	std::lock_guard lock(m_mutex);
	m_free.push(p);
}

bool TimerOverlappedPool::IsFromPool(TimerOverlapped* p) const
{
	return p >= m_rangeStart && p < m_rangeEnd;
}
