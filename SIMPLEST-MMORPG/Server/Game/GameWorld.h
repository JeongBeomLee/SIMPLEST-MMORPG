#pragma once
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <string>
#include "Map.h"
#include "SectorManager.h"
#include "Player.h"

class Session;

class GameWorld
{
public:
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

	// 조회 API
	Map& GetMap() { return m_map; }
	const Map& GetMap() const { return m_map; }
	SectorManager& GetSectorManager() { return m_sectorManager; }
	const SectorManager& GetSectorManager() const { return m_sectorManager; }

	// 플레이어 조회/전송
	Player* GetPlayer(ObjectID id);
	void SendToPlayer(ObjectID id, const void* data, uint16_t size);

private:
	GameWorld() = default;
	~GameWorld() = default;

	// 내부 헬퍼
	ObjectID AddPlayer(Session* session, const std::string& name);
	void RemovePlayer(ObjectID id);

private:
	Map m_map;
	SectorManager m_sectorManager;

	std::unordered_map<ObjectID, std::unique_ptr<Player>> m_players;
	mutable std::shared_mutex m_playersMutex;
};