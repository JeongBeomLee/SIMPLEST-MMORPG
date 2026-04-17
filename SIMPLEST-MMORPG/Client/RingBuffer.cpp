#include "RingBuffer.h"
#include <cstring>

RingBuffer::RingBuffer()
	: m_head(0)
	, m_tail(0)
{
}

int RingBuffer::GetUsedSize() const
{
	return (m_head - m_tail + BUFFER_SIZE) % BUFFER_SIZE;
}

int RingBuffer::GetFreeSize() const
{
	// 1칸은 항상 비워둠 (빈 상태와 꽉 찬 상태 구분용)
	return BUFFER_SIZE - 1 - GetUsedSize();
}

char* RingBuffer::GetWritePtr()
{
	return &m_buffer[m_head];
}

int RingBuffer::GetContiguousWriteSize() const
{
	// head부터 배열 끝까지 연속으로 쓸 수 있는 크기
	// tail이 head보다 뒤에 있으면: 배열 끝까지
	// tail이 head보다 앞에 있으면: tail 직전까지
	if (m_tail <= m_head)
	{
		int toEnd = BUFFER_SIZE - m_head;
		if (m_tail == 0)
		{
			toEnd -= 1;
		}
		return toEnd;
	}
	else
	{
		return m_tail - m_head - 1;
	}
}

void RingBuffer::OnWrite(int len)
{
	// WSARecv 완료 후 head 전진
	m_head = (m_head + len) % BUFFER_SIZE;
}

bool RingBuffer::Peek(char* dest, int len) const
{
	if (GetUsedSize() < len)
	{
		return false;
	}

	int tailToEnd = BUFFER_SIZE - m_tail;

	if (tailToEnd >= len)
	{
		memcpy(dest, &m_buffer[m_tail], len);
	}
	else
	{
		memcpy(dest, &m_buffer[m_tail], tailToEnd); // tail ~ 배열 끝
		memcpy(dest + tailToEnd, &m_buffer[0], len - tailToEnd); // 배열 처음 ~ 나머지
	}

	return true;
}

bool RingBuffer::Pop(int len)
{
	if (GetUsedSize() < len)
	{
		return false;
	}
	
	// tail 전진 (데이터 소비)
	m_tail = (m_tail + len) % BUFFER_SIZE;
	return true;
}
