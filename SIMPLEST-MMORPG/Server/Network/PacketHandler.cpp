#include "PacketHandler.h"
#include "Session.h"
#include "Protocol.h"
#include <iostream>

void PacketHandler::HandlePacket(Session* session, const char* data, uint16_t size)
{
	const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);

	switch (static_cast<PacketType>(header->type))
	{
	case PacketType::CS_LOGIN:
		std::cout << "[Session " << session->GetId() << "] LOGIN packet received" << std::endl;
		// TODO: GameWorld::ProcessLogin(session, data);
		break;
	case PacketType::CS_MOVE:
		std::cout << "[Session " << session->GetId() << "] MOVE packet received" << std::endl;
		// TODO: GameWorld::ProcessMove(session, data);
		break;
	case PacketType::CS_ATTACK:
		std::cout << "[Session " << session->GetId() << "] ATTACK packet received" << std::endl;
		// TODO: GameWorld::ProcessAttack(session, data);
		break;
	case PacketType::CS_CHAT:
		std::cout << "[Session " << session->GetId() << "] CHAT packet received" << std::endl;
		// TODO: GameWorld::ProcessChat(session, data);
		break;
	default:
		std::cout << "[Session " << session->GetId() << "] Unknown packet type: " << header->type << std::endl;
		break;
	}
}
