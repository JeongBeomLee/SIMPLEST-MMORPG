#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <string>
#include "Map.h"
#include "SectorManager.h"
#include "Player.h"
#include "Monster.h"
#include "../Network/NetworkTypes.h"

class Session;

class GameWorld
{
public:
	static constexpr ObjectID INVALID_PLAYER_ID = UINT32_MAX;

	static GameWorld& GetInstance();

	GameWorld(const GameWorld&) = delete;
	GameWorld& operator=(const GameWorld&) = delete;

	// 초기화 / 종료
	void Init();
	void Shutdown();

	// 패킷 처리 엔트리
	void ProcessLogin(Session* session, const char* data);
	void ProcessMove(Session* session, const char* data);
	void ProcessDisconnect(Session* session);
	void ProcessAttack(Session* session, const char* data);

	// 조회 API
	Map& GetMap() { return m_map; }
	const Map& GetMap() const { return m_map; }
	SectorManager& GetSectorManager() { return m_sectorManager; }
	const SectorManager& GetSectorManager() const { return m_sectorManager; }

	// 플레이어 관련
	std::shared_ptr<Player> GetPlayer(ObjectID id);
	void SendToPlayer(ObjectID id, const void* data, uint16_t size);

	// 몬스터 관련
	void SpawnMonsters();
	Monster* GetMonster(ObjectID id);
	void MoveMonster(Monster* monster, int16_t newX, int16_t newY);
	void RespawnMonster(ObjectID monsterId);
	void BroadcastAddMonster(Monster* monster);
	std::shared_ptr<Player> FindNearestPlayerInRange(int16_t x, int16_t y, int range);
	void MonsterAttackPlayer(Monster* attacker, Player* victim);

	// 타이머 이벤트 핸들러 (TimerManager -> IOCP worker -> HandleTimerEvent)
	void HandleTimerEvent(TimerType type, uint32_t targetId);

private:
	GameWorld() = default;
	~GameWorld() = default;

	// 내부 헬퍼
	ObjectID AddPlayer(Session* session, const std::string& name);
	void RemovePlayer(ObjectID id);
	void OnPlayerDied(Player* victim);
	void StopRegenAndMaybeRestart(Player* player);

	void ActivateNearbyMonsters(int16_t x, int16_t y);
	bool HasObserverNearby(Monster* monster) const;

	void OnMonsterDied(Monster* monster, const std::shared_ptr<Player>& killer);
	void SendCombatMessage(Player* receiver, ObjectID attackerId, ObjectID targetId, int32_t damage);
	void BroadcastAttackEffect(ObjectID attackerId, int16_t cx, int16_t cy);

private:
	Map m_map;
	SectorManager m_sectorManager;

	std::unordered_map<ObjectID, std::shared_ptr<Player>> m_players;
	mutable std::shared_mutex m_playersMutex;

	std::vector<std::unique_ptr<Monster>> m_monsters;
};