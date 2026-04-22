#pragma once
#include "GameObject.h"
#include "Types.h"
#include <chrono>
#include <optional>

class Monster : public GameObject
{
public:
	Monster(ObjectID id,
		const std::string& name,
		uint16_t level,
		int32_t maxHp,
		MonsterBehavior behavior,
		MonsterMovement movement,
		int16_t spawnX, int16_t spawnY);
	~Monster() override = default;

	// Getter
	MonsterBehavior GetBehavior() const { return m_behavior; }
	MonsterMovement GetMovement() const { return m_movement; }
	Position GetSpawnPos() const { return m_spawnPos; }

	// 사망 / 리스폰
	void Die();
	void Respawn();
	std::chrono::steady_clock::time_point GetDeathTime() const { return m_deathTime; }

	// 경험치 보상
	int32_t GetExpReward() const;

	// 타겟 관리 (Agro 용)
	std::optional<ObjectID> GetTargetPlayerId() const { return m_targetPlayerId; }
	void SetTargetPlayerId(ObjectID id) { m_targetPlayerId = id; }
	void ClearTarget() { m_targetPlayerId.reset(); }
	bool HasTarget() const { return m_targetPlayerId.has_value(); }

private:
	MonsterBehavior m_behavior;
	MonsterMovement m_movement;
	Position m_spawnPos;

	std::optional<ObjectID> m_targetPlayerId;

	std::chrono::steady_clock::time_point m_deathTime{};
};

