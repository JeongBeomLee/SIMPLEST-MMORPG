#pragma once

class RingBuffer
{
public:
	static constexpr int BUFFER_SIZE = 8192;

	RingBuffer();
	~RingBuffer() = default;

	// 상태 조회
	int GetUsedSize() const;
	int GetFreeSize() const;

	// WSARecv용 - 직접 버퍼에 쓰기
	char* GetWritePtr();
	int GetContiguousWriteSize() const;
	void OnWrite(int len);

	// 패킷 추출용
	bool Peek(char* dest, int len) const;
	bool Pop(int len);

private:
	char m_buffer[BUFFER_SIZE];
	int m_head; // 쓰기 위치
	int m_tail; // 읽기 위치
};