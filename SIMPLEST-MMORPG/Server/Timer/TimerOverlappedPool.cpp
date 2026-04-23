#include "TimerOverlappedPool.h"

TimerOverlappedPool::TimerOverlappedPool(size_t capacity)
{
}

TimerOverlapped* TimerOverlappedPool::Acquire()
{
    return nullptr;
}

void TimerOverlappedPool::Release(TimerOverlapped* p)
{
}

bool TimerOverlappedPool::IsFromPool(TimerOverlapped* p) const
{
    return false;
}
