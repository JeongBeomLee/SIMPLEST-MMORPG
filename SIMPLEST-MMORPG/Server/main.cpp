#include <iostream>
#include "Network/IOCPServer.h"
#include "Constants.h"
#include "Game/Map.h"

int main()
{
	// Map 로드 (TODO: GameWorld로 이전)
	auto loaded = Map::TryLoad("Data/map_obstacles.dat");
	Map map = loaded ? std::move(*loaded) : Map::CreateDefault();
	if (!loaded) {
		map.SaveToFile("Data/map_obstacles.dat");
	}

	IOCPServer server;
	if (!server.Init(SERVER_PORT))
	{
		std::cout << "Server init failed" << std::endl;
		return -1;
	}

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