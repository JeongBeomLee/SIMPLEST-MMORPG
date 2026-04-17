#include <iostream>
#include "Network/IOCPServer.h"
#include "Constants.h"

int main()
{
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