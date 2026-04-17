#include <iostream>
#include <string>
#include "NetworkClient.h"
#include "Protocol.h"
#include "Constants.h"

int main()
{
	NetworkClient client;

	if (!client.Connect("127.0.0.1", SERVER_PORT))
	{
		std::cout << "Connect failed" << std::endl;
		return -1;
	}

	// 이름 입력
	std::string name;
	std::cout << "Enter name: ";
	std::cin >> name;
	if (name.size() >= 32) {
		std::cout << "Name too long" << std::endl;
		return -1;
	}

	CS_Login pkt;
	pkt.header.size = sizeof(CS_Login);
	pkt.header.type = static_cast<uint16_t>(PacketType::CS_LOGIN);
	strncpy_s(pkt.name, sizeof(pkt.name), name.c_str(), _TRUNCATE);
	client.SendPacket(&pkt, sizeof(pkt));

	// 종료 대기
	std::cout << "Press 'q' + Enter to quit" << std::endl;
	char q;
	std::cin >> q;

	client.Disconnect();
}