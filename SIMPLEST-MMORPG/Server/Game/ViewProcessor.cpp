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

void ViewProcessor::ProcessMoveView(Player* me, int16_t oldX, int16_t oldY)
{
	if (!me)
	{
		return;
	}

	GameWorld& world = GameWorld::GetInstance();
	SectorManager& sm = world.GetSectorManager();

	Position newPos = me->GetPos();
	int16_t newX = newPos.x, newY = newPos.y;
	ObjectID myId = me->GetId();

	// 1. 두 시야 집합 계산
	auto oldNearby = sm.GetNearbyObjects(oldX, oldY);
	auto newNearby = sm.GetNearbyObjects(newX, newY);

	std::unordered_set<ObjectID> oldView;
	std::unordered_set<ObjectID> newView;

	for (ObjectID id : oldNearby)
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
			if (IsInView(oldX, oldY, op.x, op.y))
			{
				oldView.insert(id);
			}
		}
		else
		{
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
			if (IsInView(newX, newY, op.x, op.y))
			{
				newView.insert(id);
			}
		}
		else
		{
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

			SendAddObject(me, monster);
		}
	}

	// 3. 안 보이게 된 것 (oldView - newView): REMOVE 양방향
	for (ObjectID id : oldView)
	{
		if (newView.find(id) != newView.end())
		{
			continue;
		}

		SendRemoveObject(me, id);

		if (IsPlayerId(id))
		{
			auto otherPlayer = world.GetPlayer(id);
			if (otherPlayer)
			{
				SendRemoveObject(otherPlayer.get(), myId);
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
			SendMoveObject(otherPlayer.get(), me);
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