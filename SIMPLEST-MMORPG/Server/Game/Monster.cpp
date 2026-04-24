#include "Monster.h"

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