#pragma once
#include <cstdint>

class Session;

class PacketHandler
{
public:
	static void HandlePacket(Session* session, const char* data, uint16_t size);
};