#include <iostream>
#include "Network/IOCPServer.h"
#include "Game/GameWorld.h"
#include "Constants.h"

int main()
{
	// 1. GameWorld 초기화 (Map 로드 + SectorManager 준비)
	GameWorld::GetInstance().Init();

	// 2. IOCP 서버 시작
	IOCPServer server;
	if (!server.Init(SERVER_PORT))
	{
		std::cout << "Server init failed" << std::endl;
		return -1;
	}

	// 3. 종료 명령 대기
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