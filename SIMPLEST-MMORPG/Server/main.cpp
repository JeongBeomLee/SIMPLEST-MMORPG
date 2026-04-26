#include <iostream>
#include "Network/IOCPServer.h"
#include "Game/GameWorld.h"
#include "Constants.h"
#include "Timer/TimerManager.h"
#include "DB/DBManager.h"
#include "Logger.h"

int main()
{
	// GameWorld 초기화 (Map 로드 + SectorManager 준비)
	GameWorld::GetInstance().Init();

	// IOCP 서버 시작
	IOCPServer& server = IOCPServer::GetInstance();
	if (!server.Init(SERVER_PORT))
	{
		LOG_ERROR("Server init failed");
		return -1;
	}

	// DBManager 시작
	char* pwd = nullptr;
	size_t pwdLen = 0;
	_dupenv_s(&pwd, &pwdLen, "MMORPG_DB_PASSWORD");

	bool ok = DBManager::GetInstance().Init(
		server.GetIOCPHandle(),
		"localhost,1433",
		"mmorpg_dev",
		"mmorpg_user",
		pwd ? pwd : "Mmorpg!Dev123");
	free(pwd);

	if (!ok)
	{
		LOG_ERROR("[Main] DBManager init failed");
		server.ShutDown();
		return -1;
	}

	// TimerManager 시작
	TimerManager::GetInstance().Start(server.GetIOCPHandle());

	// 자동 저장 시작
	TimerManager::GetInstance().AddTimer(TimerType::DB_SAVE, 0, DB_SAVE_INTERVAL_MS);

	// 종료 명령 대기
	LOG_INFO("Press 'q' + Enter to shutdown");
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