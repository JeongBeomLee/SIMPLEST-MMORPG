#include "Monster.h"
#include "GameWorld.h"
#include "Pathfinder.h"
#include "Constants.h"
#include <random>
#include <algorithm>

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
	m_isActive.store(false, std::memory_order_relaxed); // 활성화 상태 리셋
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
		AgroPursue();
	}
}

bool Monster::TryActivate()
{
	bool expected = false;
	return m_isActive.compare_exchange_strong(expected, true);
}

void Monster::Deactivate()
{
	m_isActive.store(false, std::memory_order_relaxed);
}

bool Monster::CanAttack() const
{
	std::shared_lock lock(m_lock);
	auto now = std::chrono::steady_clock::now();
	auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastAttackTime).count();
	return diff >= ATTACK_COOLDOWN_MS;
}

void Monster::OnAttackPerformed()
{
	std::unique_lock lock(m_lock);
	m_lastAttackTime = std::chrono::steady_clock::now();
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

void Monster::AgroPursue()
{
	GameWorld& world = GameWorld::GetInstance();
	Position curPos = GetPos();

	// 현재 타겟 체크
	auto targetIdOpt = GetTargetPlayerId();
	if (targetIdOpt.has_value())
	{
		auto target = world.GetPlayer(*targetIdOpt);
		if (!target || target->IsDead())
		{
			ClearTarget();
			return;
		}

		Position targetPos = target->GetPos();
		int distMax = std::max(std::abs(targetPos.x - curPos.x), std::abs(targetPos.y - curPos.y));
		if (distMax > HALF_AGRO)
		{
			// 시야 밖으로 벗어남
			ClearTarget();
			return;
		}

		// 4방향 인접에 플레이어가 있으면 전부 공격
		std::vector<std::shared_ptr<Player>> adjacentVictims;
		adjacentVictims.reserve(4);

		for (int i = 0; i < 4; ++i)
		{
			int16_t ax = curPos.x + DX[i];
			int16_t ay = curPos.y + DY[i];

			ObjectID occupant = world.GetSectorManager().GetOccupant(ax, ay);
			if (occupant == SectorManager::INVALID_ID || occupant >= MONSTER_ID_OFFSET)
			{
				continue;
			}

			auto p = world.GetPlayer(occupant);
			if (p && !p->IsDead())
			{
				adjacentVictims.push_back(std::move(p));
			}
		}

		if (!adjacentVictims.empty())
		{
			if (CanAttack())
			{
				for (auto& victim : adjacentVictims)
				{
					world.MonsterAttackPlayer(this, victim.get());
				}
				world.BroadcastAttackEffect(GetId(), curPos.x, curPos.y);
				OnAttackPerformed();
			}
			return;  // 이동 x
		}

		// 플레이어 4방향 surround 타일 중 비점유 + 가장 가까운 타일 선택
		Position goalPos = targetPos;
		int bestDist = INT_MAX;

		for (int i = 0; i < 4; ++i)
		{
			int16_t cx = targetPos.x + DX[i];
			int16_t cy = targetPos.y + DY[i];

			if (!world.GetMap().IsWalkable(cx, cy))
			{
				continue;
			}

			ObjectID occupant = world.GetSectorManager().GetOccupant(cx, cy);
			if (occupant != SectorManager::INVALID_ID && occupant != GetId())
			{
				continue;
			}

			int dist = std::abs(cx - curPos.x) + std::abs(cy - curPos.y);
			if (dist < bestDist)
			{
				bestDist = dist;
				goalPos = { cx, cy };
			}
		}

		// A* 로 다음 칸
		auto path = Pathfinder::FindPath(curPos.x, curPos.y, goalPos.x, goalPos.y);
		if (path.empty())
		{
			return;
		}

		Position next = path.front();

		// Roaming 범위 벗어나면 이동 포기 (몬스터가 맵을 누비지 않게)
		if (!IsInRoamingRange(next.x, next.y))
		{
			return;
		}

		if (!world.GetMap().IsWalkable(next.x, next.y))
		{
			return;
		}

		world.MoveMonster(this, next.x, next.y);
		return;
	}

	// 타겟 없으면 AGRO_RANGE 안에서 탐색
	auto target = world.FindNearestPlayerInRange(curPos.x, curPos.y, HALF_AGRO);
	if (target)
	{
		SetTargetPlayerId(target->GetId());
	}
	else
	{
		// 타겟 못찾으면 다시 이동
		RoamingMove();
	}
}
