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

void ViewProcessor::SendMoveObject(Player* receiver, const Player* target)
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

		Player* other = world.GetPlayer(id);
		if (!other)
		{
			continue;
		}

		if (!IsInView(mx, my, other->GetX(), other->GetY()))
		{
			continue;
		}

		// 나에게 상대 정보, 상대에게 내 정보
		SendAddObject(me, other);
		SendAddObject(other, me);
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

		Player* other = world.GetPlayer(id);
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

		Player* other = world.GetPlayer(id);
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

		Player* other = world.GetPlayer(id);
		if (!other)
		{
			continue;
		}

		SendAddObject(me, other);
		SendAddObject(other, me);
	}

	// 3. 안 보이게 된 것 (oldView - newView): REMOVE 양방향
	for (ObjectID id : oldView)
	{
		if (newView.find(id) != newView.end())
		{
			continue;
		}

		Player* other = world.GetPlayer(id);
		if (!other)
		{
			continue;
		}

		SendRemoveObject(me, id);
		SendRemoveObject(other, myId);
	}

	// 4. 계속 보이는 것 (교집합): 상대에게만 MOVE(나)
	for (ObjectID id : newView)
	{
		if (oldView.find(id) == oldView.end())
		{
			continue;
		}

		Player* other = world.GetPlayer(id);
		if (!other)
		{
			continue;
		}

		SendMoveObject(other, me);
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

		Player* other = world.GetPlayer(id);
		if (!other)
		{
			continue;
		}

		if (!IsInView(mx, my, other->GetX(), other->GetY()))
		{
			continue;
		}

		SendRemoveObject(other, me->GetId());
	}
}

void ViewProcessor::SendAddObject(Player* receiver, const Player* target)
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
