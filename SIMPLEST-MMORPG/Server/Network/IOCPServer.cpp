#include "IOCPServer.h"
#include "../Game/GameWorld.h"
#include "../Timer/TimerManager.h"
#include <iostream>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

IOCPServer::IOCPServer()
	: m_hIOCP(NULL)
	, m_listenSocket(INVALID_SOCKET)
	, m_running(false)
{
	m_sessions.fill(nullptr);
	ZeroMemory(&m_acceptOv, sizeof(m_acceptOv));
	ZeroMemory(m_acceptBuf, sizeof(m_acceptBuf));
}

IOCPServer::~IOCPServer()
{
	ShutDown();
}

bool IOCPServer::Init(uint16_t port, int threadMultiplier)
{
	// 1. Winsock 초기화
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cout << "WSAStartUp failed" << std::endl;
		return false;
	}

	// 2. IOCP 완료 포트 생성
	m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (m_hIOCP == NULL)
	{
		std::cout << "CreateIOCompletionPort failed" << std::endl;
		return false;
	}

	// 3. 리슨 소켓 생성
	m_listenSocket = WSASocket(
		AF_INET, SOCK_STREAM, IPPROTO_TCP,
		NULL, 0, WSA_FLAG_OVERLAPPED
	);
	if (m_listenSocket == INVALID_SOCKET)
	{
		std::cout << "WSASocket failed" << std::endl;
		return false;
	}

	// 4. 주소 바인드
	sockaddr_in serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(port);

	if (bind(m_listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		std::cout << "bind failed" << std::endl;
		return false;
	}
	
	// 5. 리슨 시작
	if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		std::cout << "listen failed" << std::endl;
		return false;
	}

	// 6. 리슨 소켓을 IOCP에 연결
	CreateIoCompletionPort((HANDLE)m_listenSocket, m_hIOCP, 0, 0);

	// 7. 세선 ID 풀 초기화
	for (int i = 0; i < MAX_PLAYERS; ++i)
	{
		m_availableIds.push(i);
	}

	// 8. AcceptrEx 게시
	PostAccept();

	// 9. 워커스레드 시작
	m_running = true;
	int threadCount = std::thread::hardware_concurrency() * threadMultiplier;
	for (int i = 0; i < threadCount; ++i)
	{
		m_workerThreads.emplace_back(&IOCPServer::WorkerThread, this);
	}

	std::cout << "Server started on port " << port << std::endl;
	return true;
}

void IOCPServer::WorkerThread()
{
	while (m_running)
	{
		DWORD bytes = 0;
		ULONG_PTR key = 0;
		OVERLAPPED* overlapped = nullptr;

		BOOL ret = GetQueuedCompletionStatus(
			m_hIOCP,
			&bytes,
			&key,
			&overlapped,
			INFINITE	// 완료 통지 올 때까지 대기
		);

		// 종료 중이면 즉시 탈출
		if (!m_running)
		{
			break;
		}

		// 종료 신호 (Shutdown에서 PostQCS로 보냄)
		if (overlapped == nullptr)
		{
			break;
		}

		OverlappedEx* ovEx = reinterpret_cast<OverlappedEx*>(overlapped);

		// GQCS 실패 또는 연결 끊김
		if (ret == FALSE || (bytes == 0 && ovEx->ioType != IOType::ACCEPT && ovEx->ioType != IOType::TIMER))
		{
			Session* session = m_sessions[key];
			if (session != nullptr)
			{
				DisconnectSession(session);
			}
			continue;
		}

		switch (ovEx->ioType)
		{
		case IOType::ACCEPT:
			OnAccept(ovEx);
			break;
		case IOType::RECV:
			OnRecv(m_sessions[key], bytes);
			break;
		case IOType::SEND:
			OnSend(m_sessions[key]);
			break;
		case IOType::TIMER:
		{
			TimerOverlapped* tov = reinterpret_cast<TimerOverlapped*>(ovEx);
			GameWorld::GetInstance().HandleTimerEvent(tov->type, tov->targetId);
			TimerManager::GetInstance().ReleaseTimerOverlapped(tov);
			break;
		}
		}
	}
}

void IOCPServer::PostAccept()
{
	// 클라이언트 소켓 미리 생성
	SOCKET clientSocket = WSASocket(
		AF_INET, SOCK_STREAM, IPPROTO_TCP,
		NULL, 0, WSA_FLAG_OVERLAPPED
	);

	ZeroMemory(&m_acceptOv, sizeof(m_acceptOv));
	m_acceptOv.ioType = IOType::ACCEPT;
	m_acceptOv.socket = clientSocket;

	DWORD recvBytes = 0;

	BOOL ret = AcceptEx(
		m_listenSocket,
		clientSocket,
		m_acceptBuf,
		0, // 데이터 수신 안 함 (주소 정보만)
		sizeof(sockaddr_in) + 16, // 로컬 주소 크기
		sizeof(sockaddr_in) + 16, // 원격 주소 크기
		&recvBytes,
		&m_acceptOv.overlapped
	);

	if (ret == FALSE && WSAGetLastError() != ERROR_IO_PENDING)
	{
		std::cout << "AcceptEx failed" << std::endl;
		closesocket(clientSocket);
	}
}

void IOCPServer::OnAccept(OverlappedEx* ovEx)
{
	SOCKET clientSocket = ovEx->socket;

	// 세선 ID 할당
	int id = AllocSessionId();
	if (id == -1)
	{
		// 서버 꽉 참
		closesocket(clientSocket);
		PostAccept();
		return;
	}

	// 세선 생성 및 초기화
	Session* session = new Session();
	session->Init(id, clientSocket);
	m_sessions[id] = session;

	// 클라이언트 소켓을 IOCP에 연결 (key = 세선 ID)
	CreateIoCompletionPort(
		(HANDLE)clientSocket,
		m_hIOCP,
		(ULONG_PTR)id, // 완료키 = 세션 ID
		0
	);

	// 첫 recv 게시
	session->PostRecv();

	std::cout << "Client connected. Session ID: " << id << std::endl;

	// 다음 접속 대기
	PostAccept();
}

void IOCPServer::OnRecv(Session* session, DWORD bytes)
{
	if (session == nullptr)
	{
		return;
	}

	session->OnRecvComplete(bytes);
}

void IOCPServer::OnSend(Session* session)
{
	if (session == nullptr)
	{
		return;
	}

	session->OnSendComplete();
}

void IOCPServer::DisconnectSession(Session* session)
{
	if (session == nullptr)
	{
		return;
	}

	if (!session->TryMarkDisconnected())
	{
		return;
	}

	int id = session->GetId();

	GameWorld::GetInstance().ProcessDisconnect(session);

	session->Close();
	delete session;
	m_sessions[id] = nullptr;

	FreeSessionId(id);

	std::cout << "Client disconnected. Session ID: " << id << std::endl;
}

int IOCPServer::AllocSessionId()
{
	std::lock_guard lock(m_idMutex);

	if (m_availableIds.empty())
	{
		return -1;
	}

	int id = m_availableIds.front();
	m_availableIds.pop();
	return id;
}

void IOCPServer::FreeSessionId(int id)
{
	std::lock_guard lock(m_idMutex);
	m_availableIds.push(id);
}

void IOCPServer::ShutDown()
{
	if (!m_running)
	{
		return;
	}

	TimerManager::GetInstance().Stop();

	m_running = false;

	// 워커스레드 깨우기
	for (size_t i = 0; i < m_workerThreads.size(); ++i)
	{
		PostQueuedCompletionStatus(m_hIOCP, 0, 0, NULL);
	}

	// 워커스레드 종료 대기
	for (auto& t : m_workerThreads)
	{
		if (t.joinable())
		{
			t.join();
		}
	}

	// 리슨 소켓 닫기
	if (m_listenSocket != INVALID_SOCKET)
	{
		closesocket(m_listenSocket);
		m_listenSocket = INVALID_SOCKET;
	}

	// 세선 정리
	for (int i = 0; i < MAX_PLAYERS; ++i)
	{
		Session* session = m_sessions[i];
		if (session == nullptr) continue;

		if (session->TryMarkDisconnected()) {
			GameWorld::GetInstance().ProcessDisconnect(session);
		}
		session->Close();
		delete session;
		m_sessions[i] = nullptr;
	}

	// GameWorld 정리
	GameWorld::GetInstance().Shutdown();

	// IOCP 핸들 닫기
	if (m_hIOCP != NULL)
	{
		CloseHandle(m_hIOCP);
		m_hIOCP = NULL;
	}

	// Winsock 정리
	WSACleanup();
}