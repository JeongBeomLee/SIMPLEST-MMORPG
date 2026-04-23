#pragma once
#include <vector>
#include <queue>
#include <mutex>
#include <cstddef>
#include "../Network/NetworkTypes.h"

class TimerOverlappedPool
{
public:
	TimerOverlappedPool(size_t capacity);
	~TimerOverlappedPool() = default;

	TimerOverlappedPool(const TimerOverlappedPool&) = delete;
	TimerOverlappedPool& operator=(const TimerOverlappedPool&) = delete;

	// 풀에서 획득. 고갈 시 new fallback
	TimerOverlapped* Acquire();

	// 반환. fallback 으로 할당된 건 delete, 풀은 push
	void Release(TimerOverlapped* p);

private:
	bool IsFromPool(TimerOverlapped* p) const;

private:
	std::vector<TimerOverlapped> m_storage;
	std::queue<TimerOverlapped*> m_free;
	std::mutex m_mutex;

	// m_storage 의 주소 범위 (IsFromPool 에서 사용)
	TimerOverlapped* m_rangeStart{ nullptr };
	TimerOverlapped* m_rangeEnd{ nullptr };
};

