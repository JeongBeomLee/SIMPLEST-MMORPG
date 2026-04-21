#include "PacketHandler.h"
#include "Session.h"
#include "Protocol.h"
#include "../Game/GameWorld.h"
#include <iostream>

void PacketHandler::HandlePacket(Session* session, const char* data, uint16_t size)
{
	const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);
	if (size < sizeof(PacketHeader) || header->size != size)
	{
		std::cout << "[Session " << session->GetId() << "] Invalid packet size" << std::endl;
		return;
	}

	GameWorld& world = GameWorld::GetInstance();

	switch (static_cast<PacketType>(header->type))
	{
	case PacketType::CS_LOGIN:
		if (size != sizeof(CS_Login)) return;
		world.ProcessLogin(session, data);
		break;
	case PacketType::CS_MOVE:
		if (size != sizeof(CS_Move)) return;
		world.ProcessMove(session, data);
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
