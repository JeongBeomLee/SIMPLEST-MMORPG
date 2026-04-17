#include "Map.h"
#include "Constants.h"
#include <fstream>
#include <random>
#include <iostream>

std::optional<Map> Map::TryLoad(const char* filePath)
{
	std::ifstream fin(filePath, std::ios::binary);
	if (!fin.is_open())
	{
		return std::nullopt;
	}

	// 헤더 읽기
	uint32_t w = 0, h = 0;
	fin.read(reinterpret_cast<char*>(&w), sizeof(w));
	fin.read(reinterpret_cast<char*>(&h), sizeof(h));
	if (!fin || w != MAP_WIDTH || h != MAP_HEIGHT)
	{
		return std::nullopt;
	}

	// 장애물 데이터 읽기
	Map map;
	map.m_obstacle.resize(MAP_WIDTH * MAP_HEIGHT);
	fin.read(reinterpret_cast<char*>(map.m_obstacle.data()), map.m_obstacle.size());
	if (!fin)
	{
		return std::nullopt;
	}

	std::cout << "Map loaded: " << w << "x" << h << std::endl;
	return map;
}

Map Map::CreateDefault()
{
	Map map;
	map.GenerateDefault();
	return map;
}

bool Map::SaveToFile(const char* filePath) const
{
	std::ofstream fout(filePath, std::ios::binary);
	if (!fout.is_open())
	{
		std::cout << "SaveToFile failed to open: " << filePath << std::endl;
		return false;
	}

	uint32_t w = MAP_WIDTH;
	uint32_t h = MAP_HEIGHT;

	fout.write(reinterpret_cast<const char*>(&w), sizeof(w));
	fout.write(reinterpret_cast<const char*>(&h), sizeof(h));
	fout.write(reinterpret_cast<const char*>(m_obstacle.data()), m_obstacle.size());

	std::cout << "Map saved: " << filePath << std::endl;
	return true;
}

bool Map::IsWalkable(int x, int y) const
{
	if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT)
	{
		return false;
	}

	return m_obstacle[y * MAP_WIDTH + x] == 0;
}

void Map::GenerateDefault()
{
	m_obstacle.assign(MAP_WIDTH * MAP_HEIGHT, 0);

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);

	for (int y = 0; y < MAP_HEIGHT; ++y)
	{
		for (int x = 0; x < MAP_WIDTH; ++x)
		{
			// 스폰 지역 (0~19, 0~19) 클리어
			if (x < 20 && y < 20)
			{
				continue;
			}

			// 5% 확률 장애물
			if (dist(gen) < 0.05f)
			{
				m_obstacle[y * MAP_WIDTH + x] = 1;
			}
		}
	}

	std::cout << "Default map generated." << std::endl;
}
