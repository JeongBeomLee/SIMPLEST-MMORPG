#include "Monster.h"
#include "GameWorld.h"
#include <random>

Monster::Monster(ObjectID id,
	const std::string& name,
	uint16_t level,
	int32_t maxHp,
	MonsterBehavior behavior,
	MonsterMovement movement,
	int16_t spawnX, int16_t spawnY)
	: GameObject(id, ObjectType::MONSTER)
	, m_behavior(behavior)
	, m_movement(movement)
	, m_spawnPos{ spawnX, spawnY }
{
	m_name = name;
	m_level = level;
	m_maxHp = maxHp;
	m_hp = maxHp;
	m_pos.x = spawnX;
	m_pos.y = spawnY;
}

void Monster::Die()
{
	std::unique_lock lock(m_lock);
	m_hp = 0;
	m_targetPlayerId.reset();    // 타겟 해제
	m_deathTime = std::chrono::steady_clock::now();
}

void Monster::Respawn()
{
	std::unique_lock lock(m_lock);
	m_hp = m_maxHp;
	m_pos = m_spawnPos; // 원래 스폰 위치로
	m_targetPlayerId.reset();
}

int32_t Monster::GetExpReward() const
{
	std::shared_lock lock(m_lock);
	int32_t base = m_level * m_level * 2;

	if (m_behavior == MonsterBehavior::AGRO || m_movement == MonsterMovement::ROAMING)
	{
		base *= 2;
	}

	return base;
}

void Monster::AITick()
{
	MonsterBehavior behavior = m_behavior;
	MonsterMovement movement = m_movement;

	if (IsDead())
	{
		return;
	}

	if (movement == MonsterMovement::FIXED)
	{
		return;
	}

	if (behavior == MonsterBehavior::PEACE)
	{
		RoamingMove();
	}
	else if (behavior == MonsterBehavior::AGRO)
	{
		// AgroPursue 대신 RoamingMove 로 임시 대체
		RoamingMove();
	}
}

void Monster::RoamingMove()
{
	GameWorld& world = GameWorld::GetInstance();

	Position curPos = GetPos();

	thread_local std::mt19937 rng(std::random_device{}());
	std::uniform_int_distribution<int> dirDist(0, 3);
	int dir = dirDist(rng);

	int16_t newX = curPos.x + DX[dir];
	int16_t newY = curPos.y + DY[dir];

	Position spawnPos = m_spawnPos;
	if (!IsInRoamingRange(newX, newY))
	{
		return;
	}

	if (!world.GetMap().IsWalkable(newX, newY))
	{
		return;
	}

	world.MoveMonster(this, newX, newY);
}
