#include "GameState.h"
#include <algorithm>

GameState& GameState::GetInstance()
{
	static GameState instance;
	return instance;
}

bool GameState::Init()
{
	auto loaded = Map::TryLoad("Data/map_obstacles.dat");
	if (loaded) 
	{
		m_map = std::move(*loaded);
		return true;
	}
	return false;
}

const Map& GameState::GetMap() const
{
	return m_map;
}

void GameState::SetMyPlayer(const MyPlayer& me)
{
	std::lock_guard lock(m_mutex);
	m_me = me;
	m_loggedIn = true;
}

void GameState::SetMyName(const std::string& name)
{
	std::lock_guard lock(m_mutex);
	m_me.name = name;
}

void GameState::AddObject(const RemoteObject& obj)
{
	std::lock_guard lock(m_mutex);
	m_objects[obj.id] = obj;
}

void GameState::MoveObject(ObjectID id, int16_t x, int16_t y)
{
	std::lock_guard lock(m_mutex);
	auto it = m_objects.find(id);
	if (it != m_objects.end())
	{
		it->second.x = x;
		it->second.y = y;
	}
}

void GameState::RemoveObject(ObjectID id)
{
	std::lock_guard lock(m_mutex);
	m_objects.erase(id);
}

void GameState::MoveMyPlayer(int16_t x, int16_t y)
{
	std::lock_guard lock(m_mutex);
	m_me.x = x;
	m_me.y = y;
}

MyPlayer GameState::GetMyPlayer() const
{
	std::lock_guard lock(m_mutex);
	return m_me;
}

std::vector<RemoteObject> GameState::GetAllObjects() const
{
	std::lock_guard lock(m_mutex);
	std::vector<RemoteObject> result;
	result.reserve(m_objects.size());
	for (const auto& [id, obj] : m_objects)
	{
		result.push_back(obj);
	}
	return result;
}

void GameState::AddAttackEffect(int16_t x, int16_t y)
{
	std::lock_guard lock(m_mutex);
	m_effects.push_back({ x, y, std::chrono::steady_clock::now() });
}

std::vector<AttackEffect> GameState::GetActiveEffects()
{
	std::lock_guard lock(m_mutex);
	auto now = std::chrono::steady_clock::now();
	constexpr auto duration = std::chrono::milliseconds(400);

	// 500ms 지난 effect 제거
	m_effects.erase(
		std::remove_if(m_effects.begin(), m_effects.end(),
			[&](const AttackEffect& e) {
				return now - e.startTime > duration;
			}),
		m_effects.end());

	return m_effects;
}
