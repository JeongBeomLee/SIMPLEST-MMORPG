#include "ViewProcessor.h"
#include "GameWorld.h"
#include "Player.h"
#include "Protocol.h"
#include "../Network/Session.h"
#include <cstdlib>
#include <unordered_set>
#include <cstring>

bool ViewProcessor::IsInView(int16_t px, int16_t py, int16_t ox, int16_t oy)
{
	int dx = std::abs(px - ox);
	int dy = std::abs(py - oy);
	return dx <= HALF_VIEW && dy <= HALF_VIEW;
}

void ViewProcessor::SendAddObject(Player* receiver, const GameObject* target)
{
	if (!receiver || !target)
	{
		return;
	}

	SC_AddObject pkt;
	pkt.header.size = sizeof(pkt);
	pkt.header.type = static_cast<uint16_t>(PacketType::SC_ADD_OBJECT);
	pkt.object_id = target->GetId();
	pkt.object_type = static_cast<uint8_t>(target->GetType());
	pkt.level = target->GetLevel();
	pkt.hp = target->GetHp();
	pkt.max_hp = target->GetMaxHp();
	strncpy_s(pkt.name, sizeof(pkt.name), target->GetName().c_str(), _TRUNCATE);

	Position tp = target->GetPos();
	pkt.x = tp.x;
	pkt.y = tp.y;

	receiver->GetSession()->SendPacket(&pkt, sizeof(pkt));
}

void ViewProcessor::SendRemoveObject(Player* receiver, ObjectID targetId)
{
	if (!receiver)
	{
		return;
	}

	SC_RemoveObject pkt;
	pkt.header.size = sizeof(pkt);
	pkt.header.type = static_cast<uint16_t>(PacketType::SC_REMOVE_OBJECT);
	pkt.object_id = targetId;

	receiver->GetSession()->SendPacket(&pkt, sizeof(pkt));
}

void ViewProcessor::SendMoveObject(Player* receiver, const GameObject* target)
{
	if (!receiver || !target)
	{
		return;
	}

	SC_MoveObject pkt;
	pkt.header.size = sizeof(pkt);
	pkt.header.type = static_cast<uint16_t>(PacketType::SC_MOVE_OBJECT);
	pkt.object_id = target->GetId();

	Position tp = target->GetPos();
	pkt.x = tp.x;
	pkt.y = tp.y;

	receiver->GetSession()->SendPacket(&pkt, sizeof(pkt));
}

void ViewProcessor::SendFullView(Player* me)
{
	if (!me)
	{
		return;
	}

	GameWorld& world = GameWorld::GetInstance();

	Position myPos = me->GetPos();
	ObjectID myId = me->GetId();

	auto nearbyIds = world.GetSectorManager().GetNearbyObjects(myPos.x, myPos.y);

	for (ObjectID id : nearbyIds)
	{
		if (id == myId)
		{
			continue;
		}

		if (IsPlayerId(id))
		{
			auto otherPlayer = world.GetPlayer(id);
			if (!otherPlayer)
			{
				continue;
			}

			Position op = otherPlayer->GetPos();
			if (!IsInView(myPos.x, myPos.y, op.x, op.y))
			{
				continue;
			}

			SendAddObject(me, otherPlayer.get());
			SendAddObject(otherPlayer.get(), me);
		}
		else
		{
			Monster* monster = world.GetMonster(id);
			if (!monster)
			{
				continue;
			}

			Position mp = monster->GetPos();
			if (!IsInView(myPos.x, myPos.y, mp.x, mp.y))
			{
				continue;
			}

			SendAddObject(me, monster);
		}
	}
}

void ViewProcessor::ProcessMoveView(GameObject* moved, int16_t oldX, int16_t oldY)
{
	if (!moved)
	{
		return;
	}

	GameWorld& world = GameWorld::GetInstance();
	SectorManager& sectorManager = world.GetSectorManager();

	Position newPos = moved->GetPos();
	int16_t newX = newPos.x, newY = newPos.y;
	ObjectID movedId = moved->GetId();

	bool movedIsPlayer = (moved->GetType() == ObjectType::PLAYER);
	Player* movedPlayer = movedIsPlayer ? static_cast<Player*>(moved) : nullptr;

	// 1. 두 시야 집합 계산
	auto oldNearby = sectorManager.GetNearbyObjects(oldX, oldY);
	auto newNearby = sectorManager.GetNearbyObjects(newX, newY);

	std::unordered_set<ObjectID> oldView;
	std::unordered_set<ObjectID> newView;

	for (ObjectID id : oldNearby)
	{
		if (id == movedId)
		{
			continue;
		}

		if (IsPlayerId(id))
		{
			auto otherPlayer = world.GetPlayer(id);
			if (!otherPlayer)
			{
				continue;
			}

			Position op = otherPlayer->GetPos();
			if (IsInView(op.x, op.y, oldX, oldY))
			{
				oldView.insert(id);
			}
		}
		else
		{
			// moved 가 Monster 면 다른 Monster 는 무시 (서로 몬스터끼리 ADD/REMOVE 불필요)
			// moved 가 Player 면 Monster 도 본인 시야에 포함
			if (!movedIsPlayer)
			{
				continue;
			}

			Monster* monster = world.GetMonster(id);
			if (!monster)
			{
				continue;
			}

			Position mp = monster->GetPos();
			if (IsInView(oldX, oldY, mp.x, mp.y))
			{
				oldView.insert(id);
			}
		}
	}

	for (ObjectID id : newNearby)
	{
		if (id == movedId)
		{
			continue;
		}

		if (IsPlayerId(id))
		{
			auto otherPlayer = world.GetPlayer(id);
			if (!otherPlayer)
			{
				continue;
			}

			Position op = otherPlayer->GetPos();
			if (IsInView(op.x, op.y, newX, newY))
			{
				newView.insert(id);
			}
		}
		else
		{
			if (!movedIsPlayer)
			{
				continue;
			}

			Monster* monster = world.GetMonster(id);
			if (!monster)
			{
				continue;
			}

			Position mp = monster->GetPos();
			if (IsInView(newX, newY, mp.x, mp.y))
			{
				newView.insert(id);
			}
		}
	}

	// 2. 새로 보이는 것 (newView - oldView): ADD 양방향
	for (ObjectID id : newView)
	{
		if (oldView.find(id) != oldView.end())
		{
			continue;
		}

		if (IsPlayerId(id))
		{
			auto otherPlayer = world.GetPlayer(id);
			if (!otherPlayer)
			{
				continue;
			}

			// 관찰자에게 moved 알림
			SendAddObject(otherPlayer.get(), moved);

			// moved 가 Player 면 본인에게도 other 알림
			if (movedPlayer)
			{
				SendAddObject(movedPlayer, otherPlayer.get());
			}
		}
		else
		{
			if (!movedPlayer)
			{
				continue;
			}

			Monster* monster = world.GetMonster(id);
			if (!monster)
			{
				continue;
			}

			SendAddObject(movedPlayer, monster);
		}
	}

	// 3. 안 보이게 된 것 (oldView - newView): REMOVE 양방향
	for (ObjectID id : oldView)
	{
		if (newView.find(id) != newView.end())
		{
			continue;
		}

		if (IsPlayerId(id))
		{
			auto otherPlayer = world.GetPlayer(id);
			if (otherPlayer)
			{
				SendRemoveObject(otherPlayer.get(), movedId);
			}

			if (movedPlayer)
			{
				SendRemoveObject(movedPlayer, id);
			}
		}
		else
		{
			if (movedPlayer)
			{
				SendRemoveObject(movedPlayer, id);
			}
		}
	}

	// 4. 계속 보이는 것 (교집합): 상대에게만 MOVE(나)
	for (ObjectID id : newView)
	{
		if (oldView.find(id) == oldView.end())
		{
			continue;
		}

		if (!IsPlayerId(id))
		{
			continue;
		}

		auto otherPlayer = world.GetPlayer(id);
		if (otherPlayer)
		{
			SendMoveObject(otherPlayer.get(), moved);
		}
	}
}

void ViewProcessor::SendDisappear(Player* me)
{
	if (!me)
	{
		return;
	}

	GameWorld& world = GameWorld::GetInstance();
	Position myPos = me->GetPos();
	ObjectID myId = me->GetId();

	auto nearbyIds = world.GetSectorManager().GetNearbyObjects(myPos.x, myPos.y);

	for (ObjectID id : nearbyIds)
	{
		if (id == me->GetId())
		{
			continue;
		}

		if (!IsPlayerId(id))
		{
			continue;
		}

		auto otherPlayer = world.GetPlayer(id);
		if (!otherPlayer)
		{
			continue;
		}

		Position op = otherPlayer->GetPos();
		if (!IsInView(myPos.x, myPos.y, op.x, op.y))
		{
			continue;
		}

		SendRemoveObject(otherPlayer.get(), me->GetId());
	}
}