#pragma once
#include <vector>
#include <cstdint>
#include <optional>

class Map
{
public:
	// 정적 팩토리 메서드들
	static std::optional<Map> TryLoad(const char* filePath);
	static Map CreateDefault();

	// 저장
	bool SaveToFile(const char* filePath) const;

	// 쿼리
	bool IsWalkable(int x, int y) const;

	// 복사는 무거우니 금지, 이동만 허용
	Map(const Map&) = delete;
	Map& operator=(const Map&) = delete;
	Map(Map&&) = default;
	Map& operator=(Map&&) = default;

private:
	Map() = default;  // 팩토리에서만 생성
	void GenerateDefault();

private:
	std::vector<uint8_t> m_obstacle; // 0 = 통과, 1 = 장애물
};

