#include "GameWorld.h"
#include "../Network/Session.h"
#include "../Lua/LuaManager.h"
#include "../Timer/TimerManager.h"
#include "../DB/DBManager.h"
#include "../Network/IOCPServer.h"
#include "../Logger.h"
#include "Types.h"
#include "ViewProcessor.h"
#include <iostream>
#include <algorithm>
#include <random>

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
	// 패킷 파싱
	const CS_Login* pkt = reinterpret_cast<const CS_Login*>(data);
	std::string name(pkt->name);
	int sessionId = session->GetId();

	DBManager::GetInstance().PostLoginTask(
		[sessionId, name](DBConnection& db)
		{
			PlayerRow row;
			bool found = false;

			if (!db.LoadPlayerByName(name, row, found))
			{
				LOG_ERROR("[Login] LoadPlayer DB error for " << name);
				DBManager::GetInstance().InvokeOnIOCP([sessionId]() {
					GameWorld::GetInstance().OnLoginDBFailed(sessionId, LoginFailReason::DB_ERROR);
					});
				return;
			}

			if (!found)
			{
				int64_t newId = 0;
				if (!db.CreatePlayer(name, newId))
				{
					LOG_ERROR("[Login] CreatePlayer failed for " << name);
					DBManager::GetInstance().InvokeOnIOCP([sessionId]() {
						GameWorld::GetInstance().OnLoginDBFailed(sessionId, LoginFailReason::DB_ERROR);
						});
					return;
				}

				row.id = newId;
				row.name = name;
				row.level = 1;
				row.exp = 0;
				row.hp = 100;
				row.maxHp = 100;
				row.x = 0;
				row.y = 0;
			}

			DBManager::GetInstance().InvokeOnIOCP([sessionId, row]() {
				GameWorld::GetInstance().OnLoginDBLoaded(sessionId, row);
				});
		});
}

ObjectID GameWorld::AddPlayer(Session* session, const PlayerRow& row)
{
	ObjectID id = session->GetId();

	auto player = std::make_shared<Player>(id, session, row.name, row.id);
	player->SetLevel(row.level);
	player->SetExp(row.exp);
	player->SetMaxHp(row.maxHp);
	player->SetHp(row.hp);

	int16_t spawnX, spawnY;
	if (!m_sectorManager.AddObject(id, row.x, row.y, m_map, spawnX, spawnY))
	{
		return INVALID_PLAYER_ID;
	}

	player->SetPos(spawnX, spawnY);

	{
		std::unique_lock lock(m_playersMutex);
		m_players[id] = std::move(player);
	}

	return id;
}

void GameWorld::RemovePlayer(ObjectID id)
{
	std::unique_lock lock(m_playersMutex);
	m_players.erase(id);
}

void GameWorld::OnPlayerDied(Player* victim)
{
	Position oldPos = victim->GetPos();

	// 시야 내 다른 플레이어에게 SC_RemoveObject
	ViewProcessor::SendDisappear(victim);

	// 섹터 제거
	m_sectorManager.RemoveObject(victim->GetId(), oldPos.x, oldPos.y);

	// Player 상태 초기화 (HP 풀, exp -50%, pos = (0, 0))
	victim->Die();
	victim->StopRegen();

	// 새 위치 점유 (빈 타일 탐색)
	int16_t respawnX, respawnY;
	if (!m_sectorManager.AddObject(victim->GetId(), 0, 0, m_map, respawnX, respawnY))
	{
		LOG_ERROR("Failed to respawn player " << victim->GetId());
		return;
	}
	victim->SetPos(respawnX, respawnY);

	// 변경된 stat 알림 (HP 회복 + exp 감소)
	SC_StatChange statPkt;
	statPkt.header.size = sizeof(statPkt);
	statPkt.header.type = static_cast<uint16_t>(PacketType::SC_STAT_CHANGE);
	statPkt.object_id = victim->GetId();
	statPkt.hp = victim->GetHp();
	statPkt.max_hp = victim->GetMaxHp();
	statPkt.exp = victim->GetExp();
	statPkt.level = victim->GetLevel();
	victim->GetSession()->SendPacket(&statPkt, sizeof(statPkt));

	// 새 위치 알림 (본인에게 SC_MoveObject 형태로)
	SC_MoveObject movePkt;
	movePkt.header.size = sizeof(movePkt);
	movePkt.header.type = static_cast<uint16_t>(PacketType::SC_MOVE_OBJECT);
	movePkt.object_id = victim->GetId();
	movePkt.x = respawnX;
	movePkt.y = respawnY;
	movePkt.move_time = 0;
	victim->GetSession()->SendPacket(&movePkt, sizeof(movePkt));

	// 새 위치에서 시야 갱신
	ViewProcessor::SendFullView(victim);

	// 새 위치 주변 몬스터 활성화
	ActivateNearbyMonsters(respawnX, respawnY);

	LOG_INFO("[Combat] Player " << victim->GetId() << " died and respawned at (" << respawnX << ", " << respawnY << ")");
}

void GameWorld::StopRegenAndMaybeRestart(Player* player)
{
	player->StopRegen();

	// StopRegen 직후 피격으로 HP가 깎였는지 확인
	if (player->GetHp() < player->GetMaxHp() && player->TryStartRegen())
	{
		TimerManager::GetInstance().AddTimer(TimerType::HP_REGEN, player->GetId(), HP_REGEN_INTERVAL_MS);
	}
}

void GameWorld::ActivateNearbyMonsters(int16_t x, int16_t y)
{
	auto nearbyIds = m_sectorManager.GetNearbyObjects(x, y);
	for (ObjectID id : nearbyIds)
	{
		if (id < MONSTER_ID_OFFSET)
		{
			continue;
		}

		Monster* monster = GetMonster(id);
		if (!monster)
		{
			continue;
		}
		if (monster->IsDead())
		{
			continue;
		}

		Position mp = monster->GetPos();
		if (!ViewProcessor::IsInView(x, y, mp.x, mp.y))
		{
			continue;
		}

		if (monster->TryActivate())
		{
			// AI 타이머 등록
			TimerManager::GetInstance().AddTimer(TimerType::MONSTER_AI, id, MONSTER_AI_TICK_MS);
		}
	}
}

bool GameWorld::HasObserverNearby(Monster* monster) const
{
	Position monsterPos = monster->GetPos();
	auto nearbyIds = m_sectorManager.GetNearbyObjects(monsterPos.x, monsterPos.y);

	for (ObjectID id : nearbyIds)
	{
		if (id >= MONSTER_ID_OFFSET)
		{
			continue;
		}

		// const 메서드라 const_cast
		auto player = const_cast<GameWorld*>(this)->GetPlayer(id);
		if (!player)
		{
			continue;
		}

		Position playerPos = player->GetPos();
		if (ViewProcessor::IsInView(playerPos.x, playerPos.y, monsterPos.x, monsterPos.y))
		{
			return true;
		}
	}

	return false;
}

void GameWorld::OnMonsterDied(Monster* monster, const std::shared_ptr<Player>& killer)
{
	int32_t expReward = monster->GetExpReward();
	killer->GainExp(expReward);

	// 처치 보상
	SC_StatChange statPkt;
	statPkt.header.size = sizeof(statPkt);
	statPkt.header.type = static_cast<uint16_t>(PacketType::SC_STAT_CHANGE);
	statPkt.object_id = killer->GetId();
	statPkt.hp = killer->GetHp();
	statPkt.max_hp = killer->GetMaxHp();
	statPkt.exp = killer->GetExp();
	statPkt.level = killer->GetLevel();
	killer->GetSession()->SendPacket(&statPkt, sizeof(statPkt));

	// 시야 내 플레이어들에게 SC_RemoveObject
	Position monsterPos = monster->GetPos();
	auto nearbyIds = m_sectorManager.GetNearbyObjects(monsterPos.x, monsterPos.y);
	for (ObjectID pid : nearbyIds)
	{
		if (pid >= MONSTER_ID_OFFSET)
		{
			continue;
		}

		auto player = GetPlayer(pid);
		if (!player)
		{
			continue;
		}

		Position playerPos = player->GetPos();
		if (!ViewProcessor::IsInView(playerPos.x, playerPos.y, monsterPos.x, monsterPos.y))
		{
			continue;
		}

		SC_RemoveObject rmPkt;
		rmPkt.header.size = sizeof(rmPkt);
		rmPkt.header.type = static_cast<uint16_t>(PacketType::SC_REMOVE_OBJECT);
		rmPkt.object_id = monster->GetId();
		player->GetSession()->SendPacket(&rmPkt, sizeof(rmPkt));
	}

	// 섹터/타일 점유 해제
	m_sectorManager.RemoveObject(monster->GetId(), monsterPos.x, monsterPos.y);

	// 30초 후 리스폰
	TimerManager::GetInstance().AddTimer(TimerType::MONSTER_RESPAWN, monster->GetId(), MONSTER_RESPAWN_MS);

	LOG_INFO("[Combat] Monster " << monster->GetId()
	         << " died. Killer " << killer->GetId()
	         << " gained " << expReward << " exp.");
}

void GameWorld::SendCombatMessage(Player* receiver, ObjectID attackerId, ObjectID targetId, int32_t damage)
{
	if (!receiver)
	{
		return;
	}

	SC_CombatMessage pkt;
	pkt.header.size = sizeof(pkt);
	pkt.header.type = static_cast<uint16_t>(PacketType::SC_COMBAT_MESSAGE);
	pkt.attacker_id = attackerId;
	pkt.target_id = targetId;
	pkt.damage = damage;

	receiver->GetSession()->SendPacket(&pkt, sizeof(pkt));
}

bool GameWorld::TryClaimDbId(int64_t dbId)
{
	std::lock_guard lock(m_dbIdMutex);
	auto [it, inserted] = m_activeDbIds.insert(dbId);
	return inserted;
}

void GameWorld::ReleaseDbId(int64_t dbId)
{
	std::lock_guard lock(m_dbIdMutex);
	m_activeDbIds.erase(dbId);
}

PlayerRow GameWorld::SnapshotPlayer(const Player& player)
{
	PlayerRow row;
	row.id = player.GetDbId();
	row.name = player.GetName();
	row.level = player.GetLevel();
	row.exp = player.GetExp();
	row.hp = player.GetHp();
	row.maxHp = player.GetMaxHp();
	Position pos = player.GetPos();
	row.x = pos.x;
	row.y = pos.y;
	return row;
}

void GameWorld::BroadcastChatGlobal(const SC_Chat& pkt)
{
	std::shared_lock lock(m_playersMutex);
	for (const auto& [id, player] : m_players)
	{
		player->GetSession()->SendPacket(&pkt, sizeof(pkt));
	}
}

void GameWorld::BroadcastChatView(Player* sender, const SC_Chat& pkt)
{
	Position p = sender->GetPos();
	auto nearbyIds = m_sectorManager.GetNearbyObjects(p.x, p.y);

	for (ObjectID oid : nearbyIds)
	{
		if (oid >= MONSTER_ID_OFFSET)
		{
			continue;
		}

		auto target = GetPlayer(oid);
		if (!target)
		{
			continue;
		}

		Position tp = target->GetPos();
		if (!ViewProcessor::IsInView(tp.x, tp.y, p.x, p.y))
		{
			continue;
		}

		target->GetSession()->SendPacket(&pkt, sizeof(pkt));
	}
}

void GameWorld::BroadcastAttackEffect(ObjectID attackerId, int16_t cx, int16_t cy)
{
	SC_AttackEffect pkt;
	pkt.header.size = sizeof(pkt);
	pkt.header.type = static_cast<uint16_t>(PacketType::SC_ATTACK_EFFECT);
	pkt.attacker_id = attackerId;
	pkt.x = cx;
	pkt.y = cy;

	auto nearbyIds = m_sectorManager.GetNearbyObjects(cx, cy);
	for (ObjectID pid : nearbyIds)
	{
		if (pid >= MONSTER_ID_OFFSET)
		{
			continue;
		}

		auto player = GetPlayer(pid);
		if (!player)
		{
			continue;
		}

		Position playerPos = player->GetPos();
		if (!ViewProcessor::IsInView(playerPos.x, playerPos.y, cx, cy))
		{
			continue;
		}

		player->GetSession()->SendPacket(&pkt, sizeof(pkt));
	}
}

void GameWorld::ProcessMove(Session* session, const char* data)
{
	ObjectID id = session->GetId();
	auto player = GetPlayer(id);
	if (!player)
	{
		return;
	}

	// 쿨다운 체크
	if (!player->CanMove())
	{
		return;
	}

	// 패킷 파싱
	const CS_Move* pkt = reinterpret_cast<const CS_Move*>(data);
	Direction dir = static_cast<Direction>(pkt->direction);
	Position oldPos = player->GetPos();

	int16_t oldX = oldPos.x;
	int16_t oldY = oldPos.y;
	int16_t newX = oldX + DX[static_cast<int>(dir)];
	int16_t newY = oldY + DY[static_cast<int>(dir)];

	// 이동 가능 체크
	if (!m_map.IsWalkable(newX, newY))
	{
		return;
	}

	// 충돌 체크 + 섹터 이동
	if (!m_sectorManager.TryMoveObject(id, oldX, oldY, newX, newY))
	{
		return;
	}

	// 위치 업데이트
	player->SetPos(newX, newY);
	player->OnMoved();

	// 시야 diff 처리
	ViewProcessor::ProcessMoveView(player.get(), oldX, oldY);
	ActivateNearbyMonsters(newX, newY);

	// mover 본인에게 ack (latency 측정용 — move_time echo)
	SC_MoveObject ackPkt;
	ackPkt.header.size = sizeof(ackPkt);
	ackPkt.header.type = static_cast<uint16_t>(PacketType::SC_MOVE_OBJECT);
	ackPkt.object_id = id;
	ackPkt.x = newX;
	ackPkt.y = newY;
	ackPkt.move_time = pkt->move_time;
	session->SendPacket(&ackPkt, sizeof(ackPkt));
}

void GameWorld::ProcessTeleport(Session* session, const char* /*data*/)
{
	ObjectID id = session->GetId();
	auto player = GetPlayer(id);
	if (!player) return;

	Position oldPos = player->GetPos();
	int16_t oldX = oldPos.x;
	int16_t oldY = oldPos.y;

	// 기존 점유 해제
	m_sectorManager.RemoveObject(id, oldX, oldY);

	// 랜덤 좌표 생성 (맵 가장자리 회피 + 빈 타일 spiral search)
	thread_local std::mt19937 rng(std::random_device{}());
	std::uniform_int_distribution<int16_t> distX(10, MAP_WIDTH - 10);
	std::uniform_int_distribution<int16_t> distY(10, MAP_HEIGHT - 10);

	int16_t targetX = distX(rng);
	int16_t targetY = distY(rng);

	int16_t newX, newY;
	if (!m_sectorManager.AddObject(id, targetX, targetY, m_map, newX, newY))
	{
		// 실패 시 (0,0) 부근으로 복귀 (예외 케이스)
		m_sectorManager.AddObject(id, 0, 0, m_map, newX, newY);
	}

	player->SetPos(newX, newY);

	// 시야 갱신
	ViewProcessor::ProcessMoveView(player.get(), oldX, oldY);
	ActivateNearbyMonsters(newX, newY);

	// mover 본인에게 위치 알림 (클라가 자기 위치 갱신)
	SC_MoveObject ackPkt;
	ackPkt.header.size = sizeof(ackPkt);
	ackPkt.header.type = static_cast<uint16_t>(PacketType::SC_MOVE_OBJECT);
	ackPkt.object_id = id;
	ackPkt.x = newX;
	ackPkt.y = newY;
	ackPkt.move_time = 0;
	session->SendPacket(&ackPkt, sizeof(ackPkt));
}

void GameWorld::ProcessDisconnect(Session* session)
{
	ObjectID id = session->GetId();
	auto player = GetPlayer(id);
	if (!player)
	{
		return;
	}

	PlayerRow saveRow = SnapshotPlayer(*player);
	int64_t dbId = player->GetDbId();

	// 시야 내 플레이어에게 SC_RemoveObject 전송
	ViewProcessor::SendDisappear(player.get());

	// 섹터에서 제거
	Position dp = player->GetPos();
	m_sectorManager.RemoveObject(id, dp.x, dp.y);

	// m_players 에서 제거
	RemovePlayer(id);

	// activeDbIds 에서 해제
	ReleaseDbId(dbId);

	// 비동기 DB 저장
	DBManager::GetInstance().PostSaveTask(
		[saveRow = std::move(saveRow)](DBConnection& db)
		{
			if (!db.SavePlayer(saveRow))
			{
				LOG_ERROR("[Save] disconnect save failed for dbId=" << saveRow.id);
			}
		});
}

void GameWorld::ProcessAttack(Session* session, const char* data)
{
	ObjectID id = session->GetId();
	auto player = GetPlayer(id);
	if (!player)
	{
		return;
	}
	if (!player->CanAttack())
	{
		return;
	}

	Position playerPos = player->GetPos();
	int16_t px = playerPos.x, py = playerPos.y;
	int32_t damage = player->GetLevel() * 10;

	// 4방향 인접 검사
	for (int d = 0; d < 4; ++d)
	{
		int16_t tx = px + DX[d];
		int16_t ty = py + DY[d];

		ObjectID targetId = m_sectorManager.GetOccupant(tx, ty);
		if (targetId == SectorManager::INVALID_ID)
		{
			continue;
		}
		if (targetId < MONSTER_ID_OFFSET)
		{
			// PvP X
			continue;
		}

		Monster* target = GetMonster(targetId);
		if (!target)
		{
			continue;
		}
		if (target->IsDead())
		{
			continue;
		}

		target->TakeDamage(damage);

		SendCombatMessage(player.get(), player->GetId(), target->GetId(), damage);

		if (target->IsDead())
		{
			OnMonsterDied(target, player);
		}
	}

	BroadcastAttackEffect(player->GetId(), px, py);

	player->OnAttackPerformed();
}

void GameWorld::ProcessChat(Session* session, const char* data)
{
	const CS_Chat* pkt = reinterpret_cast<const CS_Chat*>(data);

	ObjectID id = session->GetId();
	auto sender = GetPlayer(id);
	if (!sender)
	{
		return;
	}

	if (!sender->CanChat())
	{
		return;
	}
	sender->OnChatPerformed();

	// 메시지 길이 제한 + null 종결 보장
	char safeMsg[128];
	strncpy_s(safeMsg, sizeof(safeMsg), pkt->message, _TRUNCATE);
	if (safeMsg[0] == '\0')
	{
		return;  // 빈 메시지 거부
	}

	// 응답 패킷 조립
	SC_Chat outPkt;
	outPkt.header.size = sizeof(outPkt);
	outPkt.header.type = static_cast<uint16_t>(PacketType::SC_CHAT);
	outPkt.sender_id = id;
	outPkt.channel = pkt->channel;

	std::string senderName = sender->GetName();
	strncpy_s(outPkt.name, sizeof(outPkt.name), senderName.c_str(), _TRUNCATE);
	strncpy_s(outPkt.message, sizeof(outPkt.message), safeMsg, _TRUNCATE);

	if (static_cast<ChatChannel>(pkt->channel) == ChatChannel::GLOBAL)
	{
		BroadcastChatGlobal(outPkt);
	}
	else
	{
		BroadcastChatView(sender.get(), outPkt);
	}

	LOG_DEBUG("[Chat][" << (pkt->channel == 0 ? "VIEW" : "GLOBAL") << "] " << senderName << ": " << safeMsg);
}

void GameWorld::OnLoginDBLoaded(int sessionId, const PlayerRow& row)
{
	Session* session = IOCPServer::GetInstance().GetSession(sessionId);
	if (session == nullptr)
	{
		LOG_INFO("[Login] session " << sessionId << " disconnected during DB query");
		return;
	}

	// 중복 로그인 차단
	if (!TryClaimDbId(row.id))
	{
		LOG_WARN("[Login] dbId " << row.id << " (" << row.name << ") already in use");
		OnLoginDBFailed(sessionId, LoginFailReason::DUPLICATE_LOGIN);
		return;
	}

	ObjectID id = AddPlayer(session, row);
	if (id == INVALID_PLAYER_ID)
	{
		OnLoginDBFailed(sessionId, LoginFailReason::SPAWN_FULL);
		return;
	}

	auto player = GetPlayer(id);
	if (!player)
	{
		return;
	}

	SC_LoginOk okPkt;
	okPkt.header.size = sizeof(okPkt);
	okPkt.header.type = static_cast<uint16_t>(PacketType::SC_LOGIN_OK);
	okPkt.my_id = id;
	okPkt.level = player->GetLevel();
	okPkt.exp = player->GetExp();
	okPkt.hp = player->GetHp();
	okPkt.max_hp = player->GetMaxHp();
	Position pos = player->GetPos();
	okPkt.x = pos.x;
	okPkt.y = pos.y;
	session->SendPacket(&okPkt, sizeof(okPkt));

	ViewProcessor::SendFullView(player.get());
	ActivateNearbyMonsters(pos.x, pos.y);

	// 마지막 로그인 시각 갱신
	DBManager::GetInstance().PostSaveTask(
		[dbId = row.id](DBConnection& db)
		{
			db.UpdateLastLogin(dbId);
		});

	LOG_INFO("[Login] " << row.name << " (dbId=" << row.id << ") level=" << row.level << " pos=(" << pos.x << "," << pos.y << ")");
}

void GameWorld::OnLoginDBFailed(int sessionId, LoginFailReason reason)
{
	Session* session = IOCPServer::GetInstance().GetSession(sessionId);
	if (session == nullptr)
	{
		return;
	}

	SC_LoginFail failPkt;
	failPkt.header.size = sizeof(failPkt);
	failPkt.header.type = static_cast<uint16_t>(PacketType::SC_LOGIN_FAIL);
	failPkt.reason = static_cast<uint8_t>(reason);
	session->SendPacket(&failPkt, sizeof(failPkt));
}

void GameWorld::SaveAllPlayers()
{
	std::vector<PlayerRow> snapshots;
	{
		std::shared_lock lock(m_playersMutex);
		snapshots.reserve(m_players.size());
		for (const auto& [id, player] : m_players)
		{
			snapshots.push_back(SnapshotPlayer(*player));
		}
	}

	for (auto& row : snapshots)
	{
		DBManager::GetInstance().PostSaveTask(
			[row = std::move(row)](DBConnection& db)
			{
				if (!db.SavePlayer(row))
				{
					LOG_ERROR("[Save] failed for dbId=" << row.id);
				}
			});
	}
}

std::shared_ptr<Player> GameWorld::GetPlayer(ObjectID id)
{
	std::shared_lock lock(m_playersMutex);
	auto it = m_players.find(id);
	return (it != m_players.end()) ? it->second : nullptr;
}

void GameWorld::SendToPlayer(ObjectID id, const void* data, uint16_t size)
{
	auto player = GetPlayer(id);
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

        int16_t spawnX, spawnY;
        if (!m_sectorManager.AddObject(id, data.x, data.y, m_map, spawnX, spawnY))
        {
            LOG_WARN("Failed to spawn " << data.name << " at (" << data.x << "," << data.y << ")");
            continue;
        }

        auto monster = std::make_unique<Monster>(
            id, data.name, data.level, data.maxHp,
            data.behavior, data.movement, data.x, data.y
        );
		monster->SetPos(spawnX, spawnY);

        m_monsters.push_back(std::move(monster));
    }

	LOG_INFO("Spawned " << m_monsters.size() << " monsters");
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

void GameWorld::MoveMonster(Monster* monster, int16_t newX, int16_t newY)
{
	if (!monster)
	{
		return;
	}

	Position oldPos = monster->GetPos();
	int16_t oldX = oldPos.x, oldY = oldPos.y;

	if (!m_sectorManager.TryMoveObject(monster->GetId(), oldX, oldY, newX, newY))
	{
		return;
	}

	monster->SetPos(newX, newY);
	ViewProcessor::ProcessMoveView(monster, oldX, oldY);
}

void GameWorld::RespawnMonster(ObjectID monsterId)
{
	Monster* monster = GetMonster(monsterId);
	if (!monster)
	{
		return;
	}

	Position spawnPos = monster->GetSpawnPos();

	int16_t outX, outY;
	if (!m_sectorManager.AddObject(monsterId, spawnPos.x, spawnPos.y, m_map, outX, outY))
	{
		// 자리 못 찾음 -> 재시도
		TimerManager::GetInstance().AddTimer(TimerType::MONSTER_RESPAWN, monsterId, MONSTER_RESPAWN_MS);
		return;
	}

	// 상태 초기화 (HP 풀, target reset 등)
	monster->Respawn();
	monster->SetPos(outX, outY);

	// 시야 내 플레이어에게 SC_AddObject
	BroadcastAddMonster(monster);
	if (HasObserverNearby(monster))
	{
		if (monster->TryActivate())
		{
			TimerManager::GetInstance().AddTimer(TimerType::MONSTER_AI, monsterId, MONSTER_AI_TICK_MS);
		}
	}

	LOG_INFO("[Combat] Monster " << monsterId << " respawned at (" << outX << ", " << outY << ")");
}

void GameWorld::BroadcastAddMonster(Monster* monster)
{
	Position monsterPos = monster->GetPos();

	SC_AddObject pkt;
	pkt.header.size = sizeof(pkt);
	pkt.header.type = static_cast<uint16_t>(PacketType::SC_ADD_OBJECT);
	pkt.object_id = monster->GetId();
	pkt.object_type = static_cast<uint8_t>(ObjectType::MONSTER);
	pkt.x = monsterPos.x;
	pkt.y = monsterPos.y;
	pkt.level = monster->GetLevel();
	pkt.hp = monster->GetHp();
	pkt.max_hp = monster->GetMaxHp();
	strncpy_s(pkt.name, sizeof(pkt.name), monster->GetName().c_str(), _TRUNCATE);

	auto nearbyIds = m_sectorManager.GetNearbyObjects(monsterPos.x, monsterPos.y);
	for (ObjectID pid : nearbyIds)
	{
		if (pid >= MONSTER_ID_OFFSET)
		{
			continue;
		}

		auto player = GetPlayer(pid);
		if (!player)
		{
			continue;
		}

		Position playerPos = player->GetPos();
		if (!ViewProcessor::IsInView(playerPos.x, playerPos.y, monsterPos.x, monsterPos.y))
		{
			continue;
		}

		player->GetSession()->SendPacket(&pkt, sizeof(pkt));
	}
}

std::shared_ptr<Player> GameWorld::FindNearestPlayerInRange(int16_t x, int16_t y, int range)
{
	auto nearby = m_sectorManager.GetNearbyObjects(x, y);

	std::shared_ptr<Player> nearest = nullptr;
	int minDistSq = INT_MAX;

	for (ObjectID id : nearby)
	{
		if (id >= MONSTER_ID_OFFSET)
		{
			continue;
		}

		auto player = GetPlayer(id);
		if (!player)
		{
			continue;
		}
		if (player->IsDead())
		{
			continue;
		}

		Position playerPos = player->GetPos();
		int dx = playerPos.x - x;
		int dy = playerPos.y - y;

		int distMax = std::max(std::abs(dx), std::abs(dy));
		if (distMax > range)
		{
			continue;
		}

		int distSq = dx * dx + dy * dy;
		if (distSq < minDistSq)
		{
			minDistSq = distSq;
			nearest = player;
		}
	}

	return nearest;
}

void GameWorld::MonsterAttackPlayer(Monster* attacker, Player* victim)
{
	if (!attacker || !victim)
	{
		return;
	}
	if (victim->IsDead())
	{
		return;
	}

	int32_t damage = attacker->GetLevel() * 5;
	victim->TakeDamage(damage);

	SendCombatMessage(victim, attacker->GetId(), victim->GetId(), damage);

	// HP 갱신
	SC_StatChange statPkt;
	statPkt.header.size = sizeof(statPkt);
	statPkt.header.type = static_cast<uint16_t>(PacketType::SC_STAT_CHANGE);
	statPkt.object_id = victim->GetId();
	statPkt.hp = victim->GetHp();
	statPkt.max_hp = victim->GetMaxHp();
	statPkt.exp = victim->GetExp();
	statPkt.level = victim->GetLevel();
	victim->GetSession()->SendPacket(&statPkt, sizeof(statPkt));

	// HP 회복 타이머 시작
	if (!victim->IsDead() && victim->TryStartRegen())
	{
		TimerManager::GetInstance().AddTimer(TimerType::HP_REGEN, victim->GetId(), HP_REGEN_INTERVAL_MS);
	}

	// 사망 처리
	if (victim->IsDead())
	{
		OnPlayerDied(victim);
	}
}

void GameWorld::HandleTimerEvent(TimerType type, uint32_t targetId)
{
	switch (type)
	{
	case TimerType::HP_REGEN:
	{
		auto player = GetPlayer(targetId);
		if (!player)
		{
			break;
		}

		if (player->IsDead())
		{
			player->StopRegen();
			break;
		}

		// 풀피 도달 시 종료
		if (player->GetHp() >= player->GetMaxHp())
		{
			StopRegenAndMaybeRestart(player.get());
			break;
		}

		player->RegenHP();

		SC_StatChange statPkt;
		statPkt.header.size = sizeof(statPkt);
		statPkt.header.type = static_cast<uint16_t>(PacketType::SC_STAT_CHANGE);
		statPkt.object_id = player->GetId();
		statPkt.hp = player->GetHp();
		statPkt.max_hp = player->GetMaxHp();
		statPkt.exp = player->GetExp();
		statPkt.level = player->GetLevel();
		player->GetSession()->SendPacket(&statPkt, sizeof(statPkt));

		// 풀피 도달 시 종료 더블체크
		if (player->GetHp() >= player->GetMaxHp())
		{
			StopRegenAndMaybeRestart(player.get());
			break;
		}

		// 아직 회복 필요
		TimerManager::GetInstance().AddTimer(TimerType::HP_REGEN, targetId, HP_REGEN_INTERVAL_MS);
		break;
	}
	
	case TimerType::MONSTER_AI:
	{
		Monster* monster = GetMonster(targetId);
		if (!monster)
		{
			break;
		}
		if (monster->IsDead())
		{
			break;
		}

		// 시야 내 플레이어 확인
		if (!HasObserverNearby(monster))
		{
			monster->Deactivate();
			break;
		}

		// AI 실행
		monster->AITick();
		TimerManager::GetInstance().AddTimer(TimerType::MONSTER_AI, targetId, MONSTER_AI_TICK_MS);
		break;
	}
	case TimerType::MONSTER_RESPAWN:
		RespawnMonster(targetId);
		break;
	case TimerType::DB_SAVE:
	{
		SaveAllPlayers();
		TimerManager::GetInstance().AddTimer(TimerType::DB_SAVE, 0, DB_SAVE_INTERVAL_MS);
		break;
	}
	}
}