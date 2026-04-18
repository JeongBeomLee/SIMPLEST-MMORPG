#include "SectorManager.h"
#include <mutex>
#include <algorithm>

SectorManager::SectorManager()
{
	m_sectors.reserve(SECTOR_COUNT_X * SECTOR_COUNT_Y);
	for (int i = 0; i < SECTOR_COUNT_X * SECTOR_COUNT_Y; ++i)
	{
		m_sectors.emplace_back(std::make_unique<Sector>());
	}
}

Sector& SectorManager::GetSector(int sx, int sy)
{
	return *m_sectors[sy * SECTOR_COUNT_X + sx];
}

const Sector& SectorManager::GetSector(int sx, int sy) const
{
	return *m_sectors[sy * SECTOR_COUNT_X + sx];
}

void SectorManager::AddObject(ObjectID id, int16_t x, int16_t y)
{
	SectorCoord sc = GetSectorCoord(x, y);
	Sector& sector = GetSector(sc.sx, sc.sy);

	std::unique_lock<std::shared_mutex> lock(sector.mutex);
	sector.objects.insert(id);
}

void SectorManager::RemoveObject(ObjectID id, int16_t x, int16_t y)
{
	SectorCoord sc = GetSectorCoord(x, y);
	Sector& sector = GetSector(sc.sx, sc.sy);

	std::unique_lock<std::shared_mutex> lock(sector.mutex);
	sector.objects.erase(id);
}

void SectorManager::MoveObject(ObjectID id, int16_t oldX, int16_t oldY, int16_t newX, int16_t newY)
{
	SectorCoord oldSC = GetSectorCoord(oldX, oldY);
	SectorCoord newSC = GetSectorCoord(newX, newY);

	// 섹터 안 바뀌면 스킵
	if (oldSC == newSC)
	{
		return;
	}

	Sector& oldSector = GetSector(oldSC.sx, oldSC.sy);
	Sector& newSector = GetSector(newSC.sx, newSC.sy);

	// std::lock으로 두 락 동시 획득
	std::unique_lock<std::shared_mutex> lockOld(oldSector.mutex, std::defer_lock);
	std::unique_lock<std::shared_mutex> lockNew(newSector.mutex, std::defer_lock);
	std::lock(lockOld, lockNew);

	oldSector.objects.erase(id);
	newSector.objects.insert(id);
}

std::vector<ObjectID> SectorManager::GetNearbyObjects(int16_t x, int16_t y) const
{
	std::vector<ObjectID> result;
	result.reserve(64); // 평균 예상치

	SectorCoord center = GetSectorCoord(x, y);

	for (int dy = -1; dy <= 1; ++dy)
	{
		for (int dx = -1; dx <= 1; ++dx)
		{
			int nsx = center.sx + dx;
			int nsy = center.sy + dy;

			// 범위 밖 스킵
			if (nsx < 0 || nsx >= SECTOR_COUNT_X ||
				nsy < 0 || nsy >= SECTOR_COUNT_Y)
			{
				continue;
			}
			
			const Sector& sector = GetSector(nsx, nsy);

			std::shared_lock<std::shared_mutex> lock(sector.mutex);
			for (const ObjectID& id : sector.objects)
			{
				result.push_back(id);
			}
		}
	}

	return result;
}

