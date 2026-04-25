#pragma once
#include <atomic>
#include <vector>
#include <memory>
#include <cstdint>
#include "Sector.h"
#include "Constants.h"
#include "Types.h"

class Map;

class SectorManager
{
public:
	static constexpr ObjectID INVALID_ID = UINT32_MAX;

	SectorManager();
	~SectorManager() = default;

	// 오브젝트 입출
	bool AddObject(ObjectID id, int16_t cx, int16_t cy, const Map& map, int16_t& outX, int16_t& outY);
	void RemoveObject(ObjectID id, int16_t x, int16_t y);
	bool TryMoveObject(ObjectID id, int16_t oldX, int16_t oldY, int16_t newX, int16_t newY);

	// 범위 쿼리 - 중심 좌표 기준 3x3 인접 섹터 순회
	std::vector<ObjectID> GetNearbyObjects(int16_t x, int16_t y) const;

	// lock-free 타일 점유 관리
	bool TryClaim(int16_t x, int16_t y, ObjectID id);
	void Release(int16_t x, int16_t y, ObjectID id);

	// 인접 영역 탐색 + 점유 시도. 성공한 위치 반환 (Position).
	// 실패 시 INVALID_ID 반환
	bool TryClaimNearby(ObjectID id, int16_t cx, int16_t cy, int maxRadius, const Map& map, int16_t& outX, int16_t& outY);

private:
	Sector& GetSector(int sx, int sy);
	const Sector& GetSector(int sx, int sy) const;

	void MoveObject(ObjectID id, int16_t oldX, int16_t oldY, int16_t newX, int16_t newY);

private:
	std::vector<std::unique_ptr<Sector>> m_sectors;
	std::vector<std::atomic<ObjectID>> m_tileToObject;	// 타일 -> ObjectID 매핑
};

