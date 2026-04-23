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

GameObject* ViewProcessor::GetObjectById(ObjectID id)
{
	GameWorld& world = GameWorld::GetInstance();
	if (IsPlayerId(id))
	{
		return world.GetPlayer(id);
	}
	return world.GetMonster(id);
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
	pkt.x = target->GetX();
	pkt.y = target->GetY();
	pkt.level = target->GetLevel();
	pkt.hp = target->GetHp();
	pkt.max_hp = target->GetMaxHp();
	strncpy_s(pkt.name, sizeof(pkt.name), target->GetName().c_str(), _TRUNCATE);

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
	pkt.x = target->GetX();
	pkt.y = target->GetY();

	receiver->GetSession()->SendPacket(&pkt, sizeof(pkt));
}


void ViewProcessor::SendFullView(Player* me)
{
	if (!me)
	{
		return;
	}

	GameWorld& world = GameWorld::GetInstance();
	int16_t mx = me->GetX();
	int16_t my = me->GetY();

	auto nearbyIds = world.GetSectorManager().GetNearbyObjects(mx, my);

	for (ObjectID id : nearbyIds)
	{
		if (id == me->GetId())
		{
			continue;
		}

		GameObject* other = GetObjectById(id);
		if (!other)
		{
			continue;
		}

		if (!IsInView(mx, my, other->GetX(), other->GetY()))
		{
			continue;
		}

		// 나에게 상대 정보
		SendAddObject(me, other);

		// 상대가 Player인 경우에만 역방향 전송
		if (IsPlayerId(id))
		{
			Player* otherPlayer = static_cast<Player*>(other);
			SendAddObject(otherPlayer, me);
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

	int16_t newX = me->GetX();
	int16_t newY = me->GetY();
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

		GameObject* other = GetObjectById(id);
		if (!other)
		{
			continue;
		}

		if (IsInView(oldX, oldY, other->GetX(), other->GetY()))
		{
			oldView.insert(id);
		}
	}

	for (ObjectID id : newNearby)
	{
		if (id == myId)
		{
			continue;
		}

		GameObject* other = GetObjectById(id);
		if (!other)
		{
			continue;
		}

		if (IsInView(newX, newY, other->GetX(), other->GetY()))
		{
			newView.insert(id);
		}
	}

	// 2. 새로 보이는 것 (newView - oldView): ADD 양방향
	for (ObjectID id : newView)
	{
		if (oldView.find(id) != oldView.end())
		{
			continue;
		}

		GameObject* other = GetObjectById(id);
		if (!other)
		{
			continue;
		}

		SendAddObject(me, other);

		if (IsPlayerId(id))
		{
			SendAddObject(static_cast<Player*>(other), me);
		}
	}

	// 3. 안 보이게 된 것 (oldView - newView): REMOVE 양방향
	for (ObjectID id : oldView)
	{
		if (newView.find(id) != newView.end())
		{
			continue;
		}

		GameObject* other = GetObjectById(id);
		if (!other)
		{
			continue;
		}

		SendRemoveObject(me, id);
		
		if (IsPlayerId(id))
		{
			Player* otherPlayer = world.GetPlayer(id);
			if (otherPlayer)
			{
				SendRemoveObject(otherPlayer, myId);
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

		// 몬스터는 세션이 없으므로 플레이어에게만 전송
		if (!IsPlayerId(id)) continue;

		Player* otherPlayer = world.GetPlayer(id);
		if (otherPlayer)
		{
			SendMoveObject(otherPlayer, me);
		}
	}
}

void ViewProcessor::SendDisappear(Player* me)
{
	if (!me) return;

	GameWorld& world = GameWorld::GetInstance();
	int16_t mx = me->GetX();
	int16_t my = me->GetY();

	auto nearbyIds = world.GetSectorManager().GetNearbyObjects(mx, my);

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

		Player* otherPlayer = world.GetPlayer(id);
		if (!otherPlayer)
		{
			continue;
		}

		if (!IsInView(mx, my, otherPlayer->GetX(), otherPlayer->GetY()))
		{
			continue;
		}

		SendRemoveObject(otherPlayer, me->GetId());
	}
}