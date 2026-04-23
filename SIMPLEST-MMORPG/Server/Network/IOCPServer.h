#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include "NetworkTypes.h"
#include "Session.h"
#include "Constants.h"

class IOCPServer
{
public:
	static constexpr int DEFAULT_THREAD_MULTIPLIER = 2;

	IOCPServer();
	~IOCPServer();

	bool Init(uint16_t port, int threadMultiplier = DEFAULT_THREAD_MULTIPLIER);
	void ShutDown();

	HANDLE GetIOCPHandle() const { return m_hIOCP; }

private:
	// 내부 동작
	void WorkerThread();
	void PostAccept();
	void OnAccept(OverlappedEx* ovEx);
	void OnRecv(Session* session, DWORD bytes);
	void OnSend(Session* session);
	void DisconnectSession(Session* session);

	// 세선 ID 관리
	int AllocSessionId();
	void FreeSessionId(int id);

private:
	HANDLE m_hIOCP;
	SOCKET m_listenSocket;
	std::atomic<bool> m_running;

	// 워커 스레드
	std::vector<std::thread> m_workerThreads;

	// 세선 풀
	std::array<Session*, MAX_PLAYERS> m_sessions;
	std::queue<int> m_availableIds;
	std::mutex m_idMutex;

	// AcceptEx용
	OverlappedEx m_acceptOv;
	char m_acceptBuf[128]; // AcceptEx가 요구하는 주소 정보 버퍼
};