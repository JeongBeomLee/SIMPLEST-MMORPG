#pragma once
#include <WinSock2.h>
#include <functional>

enum class IOType 
{ 
	ACCEPT, 
	RECV, 
	SEND,
	TIMER,
	DB_COMPLETE,
};

// 타이머 이벤트 종류
enum class TimerType : uint8_t
{
	HP_REGEN,
	MONSTER_AI,
	MONSTER_RESPAWN,
	DB_SAVE,
};

struct OverlappedEx
{
	WSAOVERLAPPED overlapped; // I/O 상태 + 바이트 수
	WSABUF wsaBuf; // 버퍼 포인터 + 크기
	IOType ioType; // 작업의 종류
	SOCKET socket; // AcceptEx용 클라이언트 소켓
};

// 타이머 전용 확장 (IOCPServer 가 ioType==TIMER 확인 후 cast)
struct TimerOverlapped
{
	OverlappedEx base;
	TimerType type;
	uint32_t targetId;
};

// DB 완료 콜백 (DB 워커 -> IOCP 워커로 전달)
struct DBCompletionOverlapped
{
	OverlappedEx base;
	std::function<void()> callback;
};