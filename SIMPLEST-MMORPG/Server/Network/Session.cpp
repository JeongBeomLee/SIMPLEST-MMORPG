#include "NetworkClient.h"
#include "NetworkClient.h"
#include "Session.h"
#include "Protocol.h"
#include "PacketHandler.h"
#include <cstring>

Session::Session()
	: m_id(-1)
	, m_socket(INVALID_SOCKET)
	, m_isSending(false)
{
	ZeroMemory(&m_recvOv, sizeof(m_recvOv));
	ZeroMemory(&m_sendOv, sizeof(m_sendOv));
}

Session::~Session()
{
	Close();
}

void Session::Init(int id, SOCKET socket)
{
	m_id = id;
	m_socket = socket;
	m_isSending = false;

	ZeroMemory(&m_recvOv, sizeof(m_recvOv));
	ZeroMemory(&m_sendOv, sizeof(m_sendOv));
}

void Session::Close()
{
	if (m_socket != INVALID_SOCKET)
	{
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}

	// send 큐 비우기
	std::lock_guard<std::mutex> lock(m_sendMutex);
	while (!m_sendQueue.empty())
	{
		m_sendQueue.pop();
	}
	m_isSending = false;
}

void Session::PostRecv()
{
	// recv용 overlapped 설정
	ZeroMemory(&m_recvOv.overlapped, sizeof(WSAOVERLAPPED));
	m_recvOv.ioType = IOType::RECV;
	m_recvOv.wsaBuf.buf = m_recvBuffer.GetWritePtr();
	m_recvOv.wsaBuf.len = m_recvBuffer.GetContiguousWriteSize();

	DWORD flags = 0;
	DWORD recvBytes = 0;

	int ret = WSARecv(
		m_socket,
		&m_recvOv.wsaBuf,
		1, // 버퍼 1개
		&recvBytes,
		&flags,
		&m_recvOv.overlapped,
		NULL // 완료 루틴 안 씀 (IOCP 사용)
	);

	if (ret == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			Close();
		}
	}
}

void Session::OnRecvComplete(DWORD bytes)
{
	// 1. RingBuffer에 수신 바이트 기록
	m_recvBuffer.OnWrite(bytes);

	// 2. 패킷 추출
	while (m_recvBuffer.GetUsedSize() >= sizeof(PacketHeader))
	{
		// 헤더 미리보기
		PacketHeader header;
		m_recvBuffer.Peek(reinterpret_cast<char*>(&header), sizeof(PacketHeader));

		// 패킷이 아직 덜 왔으면 대기
		if (m_recvBuffer.GetUsedSize() < header.size)
		{
			break;
		}

		// 완성된 패킷 추출
		char packetBuf[512]; // 최대 패킷 크기보다 충분히 크게
		m_recvBuffer.Peek(packetBuf, header.size);
		m_recvBuffer.Pop(header.size);

		PacketHandler::HandlePacket(this, packetBuf, header.size);
	}

	// 3. 다음 recv 대기
	PostRecv();
}

void Session::PostSend()
{
	// 큐의 맨 앞 패킷으로 WSASend
	std::vector<char>& front = m_sendQueue.front();

	ZeroMemory(&m_sendOv.overlapped, sizeof(WSAOVERLAPPED));
	m_sendOv.ioType = IOType::SEND;
	m_sendOv.wsaBuf.buf = front.data();
	m_sendOv.wsaBuf.len = static_cast<ULONG>(front.size());

	DWORD sendBytes = 0;

	int ret = WSASend(
		m_socket,
		&m_sendOv.wsaBuf,
		1,
		&sendBytes,
		0,
		&m_sendOv.overlapped,
		NULL
	);

	if (ret == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			Close();
		}
	}
}

void Session::SendPacket(const void* data, uint16_t size)
{
	std::vector<char> packet(size);
	memcpy(packet.data(), data, size);

	std::lock_guard<std::mutex> lock(m_sendMutex);
	m_sendQueue.push(std::move(packet));

	// 현재 보내는 중이 아니면 전송 시작
	if (!m_isSending)
	{
		m_isSending = true;
		PostSend();
	}
}

void Session::OnSendComplete()
{
	std::lock_guard<std::mutex> lock(m_sendMutex);

	// 완료된 패킷 제거
	if (!m_sendQueue.empty())
	{
		m_sendQueue.pop();
	}

	// 큐에 남은 게 있으면 다음 전송
	if (!m_sendQueue.empty())
	{
		PostSend();
	}
	else
	{
		m_isSending = false;
	}
}