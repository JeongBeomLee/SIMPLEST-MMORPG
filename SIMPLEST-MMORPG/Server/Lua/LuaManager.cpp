#include "LuaManager.h"
#include "../Logger.h"
#include "sol/sol.hpp"
#include <iostream>

LuaManager& LuaManager::GetInstance()
{
	static LuaManager instance;
	return instance;
}

std::vector<MonsterSpawnData> LuaManager::LoadMonsterSpawns(const char* filePath)
{
	std::vector<MonsterSpawnData> result;

	sol::state lua;
	lua.open_libraries(sol::lib::base, sol::lib::table, sol::lib::math);

	try
	{
		lua.script_file(filePath);
	}
	catch (const sol::error& e)
	{
		LOG_ERROR("Lua error: " << e.what());
		return result;
	}

	sol::table spawns = lua["monster_spawns"];
	if (!spawns.valid())
	{
		LOG_ERROR("monster_spawns table not found");
		return result;
	}

	// 각 몬스터 종류 순회
	for (auto& kv : spawns)
	{
		sol::table monster = kv.second;

		std::string name = monster["name"];
		uint16_t level = monster["level"];
		int32_t hp = monster["hp"];
		std::string behaviorStr = monster["behavior"];
		std::string movementStr = monster["movement"];

		MonsterBehavior behavior = ParseBehavior(behaviorStr);
		MonsterMovement movement = ParseMovement(movementStr);

		// 각 스폰 좌표 순회
		sol::table positions = monster["spawns"];
		if (!positions.valid())
		{
			continue;
		}

		for (auto& pos : positions) {
			sol::table coord = pos.second;
			int16_t x = coord[1];
			int16_t y = coord[2];

			MonsterSpawnData data;
			data.name = name;
			data.level = level;
			data.maxHp = hp;
			data.behavior = behavior;
			data.movement = movement;
			data.x = x;
			data.y = y;
			result.push_back(std::move(data));
		}
	}

	LOG_INFO("Loaded " << result.size() << " monster spawns");
	return result;
}

MonsterBehavior LuaManager::ParseBehavior(const std::string& s)
{
	if (s == "agro")
	{
		return MonsterBehavior::AGRO;
	}

	return MonsterBehavior::PEACE;
}

MonsterMovement LuaManager::ParseMovement(const std::string& s)
{
	if (s == "roaming")
	{
		return MonsterMovement::ROAMING;
	}

	return MonsterMovement::FIXED;
}