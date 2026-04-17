#include "NetworkClient.h"
#include "Protocol.h"
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

NetworkClient::NetworkClient()
	: m_socket(INVALID_SOCKET)
	, m_running(false)
{
}

NetworkClient::~NetworkClient()
{
	Disconnect();
}

bool NetworkClient::Connect(const char* ip, uint16_t port)
{
	// 1. Winsock 초기화
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cout << "WSAStartup failed" << std::endl;
		return false;
	}

	// 2. 소켓 생성
	m_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_socket == INVALID_SOCKET)
	{
		std::cout << "socket failed" << std::endl;
		return false;
	}

	// 3. 서버 주소 설정
	sockaddr_in serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &serverAddr.sin_addr);

	// 4. 연결
	if (connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		std::cout << "connect failed" << std::endl;
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
		return false;
	}

	// 5. 수신 스레드 시작
	m_running = true;
	m_recvThread = std::thread(&NetworkClient::RecvThread, this);

	std::cout << "Connected to " << ip << ":" << port << std::endl;
	return true;
}

void NetworkClient::Disconnect()
{
	if (!m_running)
	{
		return;
	}

	m_running = false;

	if (m_socket != INVALID_SOCKET)
	{
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}

	if (m_recvThread.joinable())
	{
		m_recvThread.join();
	}

	WSACleanup();
}

void NetworkClient::SendPacket(const void* data, uint16_t size)
{
	int sent = 0;
	const char* ptr = static_cast<const char*>(data);

	while (sent < size)
	{
		int n = send(m_socket, ptr + sent, size - sent, 0);
		if (n <= 0)
		{
			std::cout << "send failed" << std::endl;
			return;
		}
		sent += n;
	}
}

void NetworkClient::RecvThread()
{
	while (m_running)
	{
		// 1. RingBuffer 빈 공간에 직접 recv
		char* writePtr = m_recvBuffer.GetWritePtr();
		int writeSize = m_recvBuffer.GetContiguousWriteSize();

		if (writeSize == 0)
		{
			// 버퍼 꽉 참 — 비정상 (패킷 파싱이 밀림)
			std::cout << "recv buffer full" << std::endl;
			break;
		}

		int bytes = recv(m_socket, writePtr, writeSize, 0);
		if (bytes <= 0)
		{
			// 연결 끊김 또는 에러
			break;
		}

		m_recvBuffer.OnWrite(bytes);

		// 2. 패킷 추출 루프
		while (m_recvBuffer.GetUsedSize() >= sizeof(PacketHeader))
		{
			PacketHeader header;
			m_recvBuffer.Peek(reinterpret_cast<char*>(&header), sizeof(PacketHeader));

			if (m_recvBuffer.GetUsedSize() < header.size)
			{
				break;
			}

			char packetBuf[512];
			m_recvBuffer.Peek(packetBuf, header.size);
			m_recvBuffer.Pop(header.size);

			OnPacket(packetBuf, header.size);
		}
	}

	std::cout << "RecvThread terminated" << std::endl;
}

void NetworkClient::OnPacket(const char* data, uint16_t size)
{
	const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);

	switch (static_cast<PacketType>(header->type))
	{
	case PacketType::SC_LOGIN_OK:
		std::cout << "LOGIN_OK received" << std::endl;
		break;
	case PacketType::SC_LOGIN_FAIL:
		std::cout << "LOGIN_FAIL received" << std::endl;
		break;
	default:
		std::cout << "Unknown packet type: " << header->type << std::endl;
		break;
	}
}
