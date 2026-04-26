#include "PacketHandler.h"
#include "Session.h"
#include "Protocol.h"
#include "../Game/GameWorld.h"
#include "../Logger.h"
#include <iostream>

void PacketHandler::HandlePacket(Session* session, const char* data, uint16_t size)
{
	const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);
	if (size < sizeof(PacketHeader) || header->size != size)
	{
		LOG_WARN("[Session " << session->GetId() << "] Invalid packet size");
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
		if (size != sizeof(CS_Attack)) return;
		world.ProcessAttack(session, data);
		break;
	case PacketType::CS_CHAT:
		if (size != sizeof(CS_Chat)) return;
		world.ProcessChat(session, data);
		break;
	case PacketType::CS_TELEPORT:
		if (size != sizeof(CS_Teleport)) return;
		world.ProcessTeleport(session, data);
		break;
	default:
		LOG_WARN("[Session " << session->GetId() << "] Unknown packet type: " << header->type);
		break;
	}
}
