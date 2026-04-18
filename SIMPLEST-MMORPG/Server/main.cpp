#include <iostream>
#include "Network/IOCPServer.h"
#include "Constants.h"
#include "Game/Map.h"
#include "Game/SectorManager.h"

int main()
{
	SectorManager sm;
	sm.AddObject(0, 10, 10);
	sm.AddObject(1, 12, 15);
	sm.AddObject(2, 100, 100);

	auto nearby1 = sm.GetNearbyObjects(11, 11);
	std::cout << "Near (11,11): " << nearby1.size() << std::endl;

	auto nearby2 = sm.GetNearbyObjects(100, 100);
	std::cout << "Near (100,100): " << nearby2.size() << std::endl;

	sm.MoveObject(0, 10, 10, 100, 100);
	auto nearby3 = sm.GetNearbyObjects(100, 100);
	std::cout << "After move near (100,100): " << nearby3.size() << std::endl;

	sm.RemoveObject(2, 100, 100);
	auto nearby4 = sm.GetNearbyObjects(100, 100);
	std::cout << "After remove near (100,100): " << nearby4.size() << std::endl;

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