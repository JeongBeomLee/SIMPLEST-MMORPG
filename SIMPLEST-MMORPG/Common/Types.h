#pragma once
#include <cstdint> // Self-contained header
#include "Constants.h"

// 타입 별칭
using ObjectID = uint32_t;

// 구조체
struct Position 
{
	int16_t x, y;
	auto operator<=>(const Position&) const = default;
};

struct SectorCoord
{
	int sx, sy;
	auto operator<=>(const SectorCoord&) const = default;
};

// enum class
enum class Direction
{
	UP = 0,
	DOWN,
	LEFT,
	RIGHT
};

enum class MonsterBehavior
{
	PEACE = 0,
	AGRO
};

enum class MonsterMovement
{
	FIXED = 0,
	ROAMING
};

// 방향 오프셋 배열
inline constexpr int DX[] = { 0, 0, -1, +1 };
inline constexpr int DY[] = { -1, +1, 0, 0 };

// 헬퍼 함수
__forceinline SectorCoord GetSectorCoord(int16_t x, int16_t y) { return SectorCoord{ x / SECTOR_SIZE, y / SECTOR_SIZE }; }