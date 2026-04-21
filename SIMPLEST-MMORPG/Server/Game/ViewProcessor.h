#pragma once
#include <cstdint>
#include "Types.h"

class Player;

// TODO (Phase 3): Monster 추가 시 ViewProcessor 확장 필요
//   - GetNearbyObjects 결과에 Monster ID 포함됨 (ID >= MONSTER_ID_OFFSET)
//   - 플레이어 → 몬스터: SC_AddObject(몬스터) 전송, 역방향 없음 (몬스터에 세션 없음)
//   - 몬스터 이동 시 시야 내 플레이어에게 SC_MoveObject 브로드캐스트
//   - SendAddObject/SendMoveObject 가 Player 뿐 아니라 GameObject 를 받도록 일반화 고려
class ViewProcessor
{
public:
	// 로그인: 내가 보는 오브젝트들 받기 + 내 모습을 주변에 알리기
	static void SendFullView(Player* me);

	// 이동: 시야 diff 계산 후 ADD/REMOVE/MOVE 패킷 전송
	//   oldX/oldY 는 이동 전 위치 (이미 me 의 위치는 newX/newY 로 갱신된 상태)
	static void ProcessMoveView(Player* me, int16_t oldX, int16_t oldY);

	// 로그아웃/사망: 주변 플레이어에게 내가 사라졌음을 알림
	static void SendDisappear(Player* me);

private:
	// 시야 판정: 중심 (px, py) 와 대상 (ox, oy) 가 15×15 시야 내인가
	static bool IsInView(int16_t px, int16_t py, int16_t ox, int16_t oy);

	// 헬퍼: SC_AddObject 패킷 생성 + 전송 대상 Player 에게 보내기
	static void SendAddObject(Player* receiver, const Player* target);

	// 헬퍼: SC_RemoveObject 패킷 생성 + 전송
	static void SendRemoveObject(Player* receiver, ObjectID targetId);

	// 헬퍼: SC_MoveObject 패킷 생성 + 전송
	static void SendMoveObject(Player* receiver, const Player* target);
};

