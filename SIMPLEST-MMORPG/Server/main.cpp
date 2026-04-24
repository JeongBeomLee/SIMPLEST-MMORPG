#include <iostream>
#include "Network/IOCPServer.h"
#include "Game/GameWorld.h"
#include "Constants.h"
#include "Timer/TimerManager.h"

int main()
{
	// GameWorld 초기화 (Map 로드 + SectorManager 준비)
	GameWorld::GetInstance().Init();

	// IOCP 서버 시작
	IOCPServer server;
	if (!server.Init(SERVER_PORT))
	{
		std::cout << "Server init failed" << std::endl;
		return -1;
	}

	// TimerManager 시작
	TimerManager::GetInstance().Start(server.GetIOCPHandle());
	GameWorld::GetInstance().StartAITimer();

	// 종료 명령 대기
	std::cout << "Press 'q' + Enter to shutdown" << std::endl;
	while (true)
	{
		char c;
		std::cin >> c;
		if (c == 'q')
		{
			break;
		}
	}

	server.ShutDown();
}