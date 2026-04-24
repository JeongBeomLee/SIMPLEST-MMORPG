#pragma once
#include <cstdint>
#include "Types.h"

class GameObject;
class Player;

class ViewProcessor
{
public:
	// 로그인: 내가 보는 오브젝트들 받기 + 내 모습을 주변에 알리기
	static void SendFullView(Player* me);

	// 이동: 시야 diff 계산 후 ADD/REMOVE/MOVE 패킷 전송
	//   oldX/oldY 는 이동 전 위치 (이미 me 의 위치는 newX/newY 로 갱신된 상태)
	static void ProcessMoveView(GameObject* moved, int16_t oldX, int16_t oldY);

	// 로그아웃/사망: 주변 플레이어에게 내가 사라졌음을 알림
	static void SendDisappear(Player* me);

	// 시야 판정: 중심 (px, py) 와 대상 (ox, oy) 가 15×15 시야 내인가
	static bool IsInView(int16_t px, int16_t py, int16_t ox, int16_t oy);

private:
	// ID 가 플레이어인지 여부
	static bool IsPlayerId(ObjectID id) { return id < MONSTER_ID_OFFSET; }

	// 패킷 전송 헬퍼
	static void SendAddObject(Player* receiver, const GameObject* target);
	static void SendRemoveObject(Player* receiver, ObjectID targetId);
	static void SendMoveObject(Player* receiver, const GameObject* target);
};

