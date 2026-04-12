#pragma once
#include <WinSock2.h>

enum class IOType 
{ 
	ACCEPT, 
	RECV, 
	SEND 
};

struct OverlappedEx
{
	WSAOVERLAPPED overlapped; // I/O 상태 + 바이트 수
	WSABUF wsaBuf; // 버퍼 포인터 + 크기
	IOType ioType; // 작업의 종류
	SOCKET socket; // AcceptEx용 클라이언트 소켓
};