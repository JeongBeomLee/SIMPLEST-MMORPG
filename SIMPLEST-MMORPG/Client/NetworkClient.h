#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>
#include <atomic>
#include <cstdint>
#include "RingBuffer.h"

class NetworkClient
{
public:
	NetworkClient();
	~NetworkClient();
	bool Connect(const char* ip, uint16_t port);
	void Disconnect();
	void SendPacket(const void* data, uint16_t size);

private:
	void RecvThread();
	void OnPacket(const char* data, uint16_t size);

private:
	SOCKET m_socket;
	std::thread m_recvThread;
	RingBuffer m_recvBuffer;
	std::atomic<bool> m_running;
};

