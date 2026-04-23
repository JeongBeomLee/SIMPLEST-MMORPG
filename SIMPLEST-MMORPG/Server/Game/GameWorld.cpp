#include "GameWorld.h"
#include "../Network/Session.h"
#include "../Lua/LuaManager.h"
#include "Protocol.h"
#include "Types.h"
#include "ViewProcessor.h"
#include <iostream>

GameWorld& GameWorld::GetInstance()
{
	static GameWorld instance;
	return instance;
}

void GameWorld::Init()
{
	auto loaded = Map::TryLoad("Data/map_obstacles.dat");
	if (loaded)
	{
		m_map = std::move(*loaded);
	}
	else
	{
		m_map = Map::CreateDefault();
		m_map.SaveToFile("Data/map_obstacles.dat");
	}

	SpawnMonsters();
}

void GameWorld::Shutdown()
{
	std::unique_lock lock(m_playersMutex);
	m_players.clear();
}

void GameWorld::ProcessLogin(Session* session, const char* data)
{
	// 1. 패킷 파싱
	const CS_Login* pkt = reinterpret_cast<const CS_Login*>(data);
	std::string name(pkt->name);

	// 2. 플레이어 생성 + 월드 등록
	ObjectID id = AddPlayer(session, name);

	// 3. SC_LoginOk 패킷 전송
	Player* player = GetPlayer(id);
	if (!player)
	{
		return;
	}

	SC_LoginOk okPkt;
	okPkt.header.size = sizeof(okPkt);
	okPkt.header.type = static_cast<uint16_t>(PacketType::SC_LOGIN_OK);
	okPkt.my_id = id;
	okPkt.x = player->GetX();
	okPkt.y = player->GetY();
	okPkt.level = player->GetLevel();
	okPkt.exp = player->GetExp();
	okPkt.hp = player->GetHp();
	okPkt.max_hp = player->GetMaxHp();
	session->SendPacket(&okPkt, sizeof(okPkt));

	// 4. 시야 내 객체들을 나에게 + 나를 시야 내 플레이어에게
	ViewProcessor::SendFullView(player);
}

ObjectID GameWorld::AddPlayer(Session* session, const std::string& name)
{
	ObjectID id = session->GetId();   // 세션 ID = Player ID

	auto player = std::make_unique<Player>(id, session, name);
	Position spawnPos = { 0, 0 };  // 초기 스폰 위치
	player->SetPos(spawnPos.x, spawnPos.y);

	{
		std::unique_lock lock(m_playersMutex);
		m_players[id] = std::move(player);
	}

	m_sectorManager.AddObject(id, spawnPos.x, spawnPos.y);

	return id;
}

void GameWorld::RemovePlayer(ObjectID id)
{
	std::unique_lock lock(m_playersMutex);
	m_players.erase(id);
}

void GameWorld::ProcessMove(Session* session, const char* data)
{
	ObjectID id = session->GetId();
	Player* player = GetPlayer(id);
	if (!player)
	{
		return;
	}

	// 1. 쿨다운 체크
	if (!player->CanMove())
	{
		return;
	}

	// 2. 패킷 파싱
	const CS_Move* pkt = reinterpret_cast<const CS_Move*>(data);
	Direction dir = static_cast<Direction>(pkt->direction);

	int16_t oldX = player->GetX();
	int16_t oldY = player->GetY();
	int16_t newX = oldX + DX[static_cast<int>(dir)];
	int16_t newY = oldY + DY[static_cast<int>(dir)];

	// 3. 이동 가능 체크
	if (!m_map.IsWalkable(newX, newY))
	{
		return;
	}

	// 4. 위치 업데이트
	player->SetPos(newX, newY);
	player->OnMoved();

	// 5. 섹터 이동
	m_sectorManager.MoveObject(id, oldX, oldY, newX, newY);

	// 6. 시야 diff 처리
	ViewProcessor::ProcessMoveView(player, oldX, oldY);
}

void GameWorld::ProcessDisconnect(Session* session)
{
	ObjectID id = session->GetId();
	Player* player = GetPlayer(id);
	if (!player)
	{
		return;
	}

	// 1. 시야 내 플레이어에게 SC_RemoveObject 전송
	ViewProcessor::SendDisappear(player);

	// 2. 섹터에서 제거
	m_sectorManager.RemoveObject(id, player->GetX(), player->GetY());

	// 3. m_players 에서 제거
	RemovePlayer(id);
}

Player* GameWorld::GetPlayer(ObjectID id)
{
	std::shared_lock lock(m_playersMutex);
	auto it = m_players.find(id);
	return (it != m_players.end()) ? it->second.get() : nullptr;
}

void GameWorld::SendToPlayer(ObjectID id, const void* data, uint16_t size)
{
	Player* player = GetPlayer(id);
	if (!player)
	{
		return;
	}
	player->GetSession()->SendPacket(data, size);
}

void GameWorld::SpawnMonsters()
{
	auto spawns = LuaManager::GetInstance().LoadMonsterSpawns("Scripts/monster_spawn.lua");

	m_monsters.reserve(spawns.size());

	for (size_t i = 0; i < spawns.size(); ++i)
	{
		const auto& data = spawns[i];
		ObjectID id = MONSTER_ID_OFFSET + static_cast<ObjectID>(i);

		auto monster = std::make_unique<Monster>(
			id,
			data.name,
			data.level,
			data.maxHp,
			data.behavior,
			data.movement,
			data.x, data.y
		);

		// 섹터에 등록
		m_sectorManager.AddObject(id, data.x, data.y);

		m_monsters.push_back(std::move(monster));
	}

	std::cout << "Spawned " << m_monsters.size() << " monsters" << std::endl;
}

Monster* GameWorld::GetMonster(ObjectID id)
{
	if (id < MONSTER_ID_OFFSET)
	{
		return nullptr;
	}

	size_t index = id - MONSTER_ID_OFFSET;
	if (index >= m_monsters.size())
	{
		return nullptr;
	}

	return m_monsters[index].get();
}
