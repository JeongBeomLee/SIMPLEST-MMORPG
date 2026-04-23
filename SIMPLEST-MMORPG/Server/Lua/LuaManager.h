#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "Types.h"

// 몬스터 스폰 정보 — Lua 에서 파싱한 결과
struct MonsterSpawnData
{
	std::string name;
	uint16_t level;
	int32_t maxHp;
	MonsterBehavior behavior;
	MonsterMovement movement;
	int16_t x;
	int16_t y;
};

class LuaManager
{
public:
	static LuaManager& GetInstance();

	LuaManager(const LuaManager&) = delete;
	LuaManager& operator=(const LuaManager&) = delete;

	// Lua 스크립트 로드 후 스폰 데이터 리스트 반환
	std::vector<MonsterSpawnData> LoadMonsterSpawns(const char* filePath);

private:
	LuaManager() = default;
	~LuaManager() = default;

	MonsterBehavior ParseBehavior(const std::string& s);
	MonsterMovement ParseMovement(const std::string& s);
};