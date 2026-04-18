#pragma once
#include <cstdint>

// 서버 포트
constexpr uint16_t SERVER_PORT = 9000;

// 맵 관련
constexpr int MAP_WIDTH = 2000;
constexpr int MAP_HEIGHT = 2000;
constexpr int SECTOR_SIZE = 20;
constexpr int SECTOR_COUNT_X = MAP_WIDTH / SECTOR_SIZE;
constexpr int SECTOR_COUNT_Y = MAP_HEIGHT / SECTOR_SIZE;

// 플레이어 최대 레벨
constexpr uint16_t MAX_LEVEL = 20;

// 플레이어 시야 관련
constexpr int VIEW_RANGE = 15;
constexpr int HALF_VIEW = VIEW_RANGE / 2;

// AI 시야 관련
constexpr int AGRO_RANGE = 11;
constexpr int HALF_AGRO = AGRO_RANGE / 2;
constexpr int ROAMING_RANGE = 20;

// 최대 동접 및 몬스터 수 관련
constexpr int MAX_PLAYERS = 3000;
constexpr int MAX_MONSTERS = 200000;
constexpr uint32_t MONSTER_ID_OFFSET = 1000000;

// 타이머 관련
constexpr int MOVE_COOLDOWN_MS = 1000;
constexpr int ATTACK_COOLDOWN_MS = 1000;
constexpr int HP_REGEN_INTERVAL_MS = 5000;
constexpr int MONSTER_RESPAWN_MS = 30000;
constexpr int MONSTER_AI_TICK_MS = 1000;
constexpr int DB_SAVE_INTERVAL_MS = 60000;
constexpr float HP_REGEN_RATE = 0.10f;
constexpr float DEATH_EXP_PENALTY = 0.50f;