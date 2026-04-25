#include "SectorManager.h"
#include "../Common/Map.h"
#include <mutex>
#include <algorithm>

SectorManager::SectorManager()
{
	m_sectors.reserve(SECTOR_COUNT_X * SECTOR_COUNT_Y);
	for (int i = 0; i < SECTOR_COUNT_X * SECTOR_COUNT_Y; ++i)
	{
		m_sectors.emplace_back(std::make_unique<Sector>());
	}

	m_tileToObject = std::vector<std::atomic<ObjectID>>(MAP_WIDTH * MAP_HEIGHT);
	for (auto& t : m_tileToObject)
	{
		t.store(INVALID_ID, std::memory_order_relaxed);
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

bool SectorManager::AddObject(ObjectID id, int16_t cx, int16_t cy, const Map& map, int16_t& outX, int16_t& outY)
{
	if (!TryClaimNearby(id, cx, cy, 20, map, outX, outY))
	{
		return false;
	}

	SectorCoord sc = GetSectorCoord(outX, outY);
	Sector& sector = GetSector(sc.sx, sc.sy);
	std::unique_lock<std::shared_mutex> lock(sector.mutex);
	sector.objects.insert(id);

	return true;
}

void SectorManager::RemoveObject(ObjectID id, int16_t x, int16_t y)
{
	Release(x, y, id);

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

ObjectID SectorManager::GetOccupant(int16_t x, int16_t y) const
{
	if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT)
	{
		return INVALID_ID;
	}

	size_t idx = static_cast<size_t>(y) * MAP_WIDTH + x;
	return m_tileToObject[idx].load();
}

bool SectorManager::TryClaim(int16_t x, int16_t y, ObjectID id)
{
	if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT)
	{
		return false;
	}
	
	size_t idx = static_cast<size_t>(y) * MAP_WIDTH + x;
	ObjectID expected = INVALID_ID;
	return m_tileToObject[idx].compare_exchange_strong(expected, id);
}

void SectorManager::Release(int16_t x, int16_t y, ObjectID id)
{
	if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT)
	{
		return;
	}

	size_t idx = static_cast<size_t>(y) * MAP_WIDTH + x;
	ObjectID expected = id;
	m_tileToObject[idx].compare_exchange_strong(expected, INVALID_ID);
}

bool SectorManager::TryClaimNearby(ObjectID id, int16_t cx, int16_t cy, int maxRadius, const Map& map, int16_t& outX, int16_t& outY)
{
	for (int radius = 0; radius <= maxRadius; ++radius)
	{
		for (int dy = -radius; dy <= radius; ++dy)
		{
			for (int dx = -radius; dx <= radius; ++dx)
			{
				if (radius > 0 && std::abs(dy) != radius && std::abs(dx) != radius)
				{
					continue;
				}

				int16_t x = cx + dx;
				int16_t y = cy + dy;

				// 맵 범위 + 장애물 체크
				if (!map.IsWalkable(x, y))
				{
					continue;
				}

				// 점유 시도 (CAS)
				if (TryClaim(x, y, id))
				{
					outX = x;
					outY = y;
					return true;
				}
			}
		}
	}

	return false;   // 반경 안에 빈 타일 없음
}

bool SectorManager::TryMoveObject(ObjectID id, int16_t oldX, int16_t oldY, int16_t newX, int16_t newY)
{
	if (oldX == newX && oldY == newY)
	{
		return true;
	}

	// 새 위치 점유 시도
	if (!TryClaim(newX, newY, id))
	{
		return false;   // 이미 누군가 있음
	}

	// 기존 위치 해제
	Release(oldX, oldY, id);

	// 섹터 이동
	MoveObject(id, oldX, oldY, newX, newY);

	return true;
}

