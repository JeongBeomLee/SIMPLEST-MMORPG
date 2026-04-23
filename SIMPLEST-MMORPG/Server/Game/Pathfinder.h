#pragma once
#include <vector>
#include "Types.h"

class Pathfinder
{
public:
	// A* 경로 탐색
	static std::vector<Position> FindPath(
		int16_t startX, int16_t startY,
		int16_t goalX, int16_t goalY,
		int maxNodes = 25);
};