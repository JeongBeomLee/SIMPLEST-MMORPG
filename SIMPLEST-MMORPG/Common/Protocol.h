#pragma once
#include <cstdint>

enum class PacketType : uint16_t
{
	CS_LOGIN = 1001,
	CS_MOVE,
	CS_ATTACK,
	CS_CHAT,
	SC_LOGIN_OK = 2001,
	SC_LOGIN_FAIL,
	SC_ADD_OBJECT,
	SC_REMOVE_OBJECT,
	SC_MOVE_OBJECT,
	SC_STAT_CHANGE,
	SC_CHAT,
	SC_COMBAT_MESSAGE,
	SC_ATTACK_EFFECT
};

// 1바이트 정렬 강제
#pragma pack(push, 1)

struct PacketHeader
{
	uint16_t size;
	uint16_t type;
};

struct CS_Login
{
	PacketHeader header;
	char name[32];
};

struct CS_Move
{
	PacketHeader header;
	uint8_t direction;
};

struct CS_Attack
{
	PacketHeader header;
};

struct CS_Chat
{
	PacketHeader header;
	char message[128];
};

struct SC_LoginOk
{
	PacketHeader header;
	uint32_t my_id;
	int16_t x, y;
	uint16_t level;
	uint32_t exp;
	int32_t hp, max_hp;
};

enum class LoginFailReason : uint8_t
{
	UNKNOWN = 0,
	DUPLICATE_LOGIN = 1, // 같은 캐릭터 이미 접속 중
	DB_ERROR = 2, // LoadPlayer/CreatePlayer 실패
	SPAWN_FULL = 3, // 빈 타일 못 찾음
};

struct SC_LoginFail
{
	PacketHeader header;
	uint8_t reason;
};

struct SC_AddObject
{
	PacketHeader header;
	uint32_t object_id;
	int16_t x, y;
	uint8_t object_type;
	uint16_t level;
	int32_t hp, max_hp;
	char name[32];
};

struct SC_RemoveObject
{
	PacketHeader header;
	uint32_t object_id;
};

struct SC_MoveObject
{
	PacketHeader header;
	uint32_t object_id;
	int16_t x, y;
};

struct SC_StatChange
{
	PacketHeader header;
	uint32_t object_id;
	int32_t hp, max_hp;
	uint32_t exp;
	uint16_t level;
};

struct SC_Chat
{
	PacketHeader header;
	uint32_t sender_id;
	char name[32];
	char message[128];
};

struct SC_CombatMessage
{
	PacketHeader header;
	uint32_t attacker_id;
	uint32_t target_id;
	int32_t damage;
};

struct SC_AttackEffect
{
	PacketHeader header;
	uint32_t attacker_id;
	int16_t x;
	int16_t y;
};

#pragma pack(pop)