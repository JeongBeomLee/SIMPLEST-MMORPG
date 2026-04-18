#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include "Sector.h"
#include "Constants.h"
#include "Types.h"

class SectorManager
{
public:
	SectorManager();
	~SectorManager() = default;

	// 오브젝트 입출
	void AddObject(ObjectID id, int16_t x, int16_t y);
	void RemoveObject(ObjectID id, int16_t x, int16_t y);
	void MoveObject(ObjectID id, int16_t oldX, int16_t oldY, int16_t newX, int16_t newY);

	// 범위 쿼리 - 중심 좌표 기준 3x3 인접 섹터 순회
	std::vector<ObjectID> GetNearbyObjects(int16_t x, int16_t y) const;

private:
	Sector& GetSector(int sx, int sy);
	const Sector& GetSector(int sx, int sy) const;

private:
	std::vector<std::unique_ptr<Sector>> m_sectors;
};

