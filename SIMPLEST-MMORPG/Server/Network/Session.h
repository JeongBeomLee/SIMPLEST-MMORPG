#pragma once
#include <WinSock2.h>
#include <mutex>
#include <queue>
#include <vector>
#include <atomic>
#include "NetworkTypes.h"
#include "RingBuffer.h"

class Session
{
public:
	Session();
	~Session();

	void Init(int id, SOCKET socket);
	void Close();

	// 비동기 I/O
	void PostRecv();
	void OnRecvComplete(DWORD bytes);
	void SendPacket(const void* data, uint16_t size);
	void OnSendComplete();

	// Getter
	int GetId() const { return m_id; }
	SOCKET GetSocket() const { return m_socket; }

	// 중복 Disconnect Guard
	bool TryMarkDisconnected();

private:
	void PostSend();

private:
	int m_id;
	SOCKET m_socket;

	// 수신
	RingBuffer m_recvBuffer;
	OverlappedEx m_recvOv;

	// 송신
	OverlappedEx m_sendOv;
	std::queue<std::vector<char>> m_sendQueue;
	std::mutex m_sendMutex;
	bool m_isSending;

	std::atomic<bool> m_disconnected{ false };
};