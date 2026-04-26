#define WIN32_LEAN_AND_MEAN

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <array>
#include <cstring>

#include "../Common/Protocol.h"
#include "../Common/Constants.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std::chrono;

// ============================================================
// 상수
// ============================================================
static constexpr int MAX_TEST       = 1000;            // 동접 목표
static constexpr int MAX_CLIENTS    = MAX_TEST + 500;  // 여유
static constexpr int MAX_BUFF_SIZE  = 1024;            // recv 임시 버퍼
static constexpr int RECV_ACCUM_CAP = 2048;            // 누적 버퍼
static constexpr int INVALID_ID     = -1;
static constexpr int ACCEPT_DELAY   = 50;              // ms 새 봇 추가 간격
static constexpr int DELAY_LIMIT    = 100;             // ms 한계 (이상이면 신규 접속 stop)
static constexpr int DELAY_LIMIT2   = 150;             // ms 임계 (이상이면 봇 줄임)

enum OPTYPE { OP_RECV, OP_SEND };

// ============================================================
// 자료구조
// ============================================================
struct OverlappedEx
{
	WSAOVERLAPPED over;
	WSABUF        wsabuf;
	unsigned char iocp_buf[MAX_BUFF_SIZE];
	OPTYPE        event_type;
};

struct CLIENT
{
	int                id;          // 서버가 알려준 my_id (object_id)
	int                x;
	int                y;
	std::atomic_bool   connected;

	SOCKET             socket;
	OverlappedEx       recv_over;

	// 누적 수신 버퍼 (헤더 분할 대비)
	unsigned char      recv_accum[RECV_ACCUM_CAP];
	int                recv_accum_size;

	high_resolution_clock::time_point last_move_time;
};

// ============================================================
// 전역 상태
// ============================================================
static HANDLE                                  g_hiocp;
static std::array<CLIENT, MAX_CLIENTS>         g_clients;
static std::array<int, MAX_PLAYERS>            client_map;        // [serverId] → bot index
static std::atomic_int                         num_connections{ 0 };
static std::atomic_int                         client_to_close{ 0 };
static high_resolution_clock::time_point       last_connect_time;

static std::vector<std::thread>                worker_threads;
static std::thread                             test_thread;
static std::atomic_bool                        g_running{ true };

static float                                   point_cloud[MAX_TEST * 2];

// 외부 노출
int                global_delay = 0;
std::atomic_int    active_clients{ 0 };

// ============================================================
// 헬퍼
// ============================================================
static uint64_t NowMs()
{
	return duration_cast<milliseconds>(
		high_resolution_clock::now().time_since_epoch()).count();
}

static void DisconnectClient(int ci)
{
	bool expected = true;
	if (g_clients[ci].connected.compare_exchange_strong(expected, false))
	{
		closesocket(g_clients[ci].socket);
		active_clients--;
	}
}

static void SendPacket(int ci, void* packet, uint16_t size)
{
	OverlappedEx* over = new OverlappedEx;
	over->event_type = OP_SEND;
	memcpy(over->iocp_buf, packet, size);
	ZeroMemory(&over->over, sizeof(over->over));
	over->wsabuf.buf = reinterpret_cast<CHAR*>(over->iocp_buf);
	over->wsabuf.len = size;

	int ret = WSASend(g_clients[ci].socket, &over->wsabuf, 1, NULL, 0,
	                  &over->over, NULL);
	if (ret != 0 && WSAGetLastError() != WSA_IO_PENDING)
	{
		delete over;
		DisconnectClient(ci);
	}
}

// ============================================================
// 패킷 처리
// ============================================================
static void ProcessPacket(int ci, const unsigned char* packet)
{
	const PacketHeader* hdr = reinterpret_cast<const PacketHeader*>(packet);
	PacketType ptype = static_cast<PacketType>(hdr->type);

	switch (ptype)
	{
	case PacketType::SC_LOGIN_OK:
	{
		const SC_LoginOk* p = reinterpret_cast<const SC_LoginOk*>(packet);
		g_clients[ci].connected = true;
		g_clients[ci].id        = static_cast<int>(p->my_id);
		g_clients[ci].x         = p->x;
		g_clients[ci].y         = p->y;
		if (p->my_id < MAX_PLAYERS)
		{
			client_map[p->my_id] = ci;
		}
		active_clients++;
		break;
	}

	case PacketType::SC_MOVE_OBJECT:
	{
		const SC_MoveObject* p = reinterpret_cast<const SC_MoveObject*>(packet);
		if (p->object_id < MAX_PLAYERS)
		{
			int my_idx = client_map[p->object_id];
			if (my_idx != INVALID_ID)
			{
				g_clients[my_idx].x = p->x;
				g_clients[my_idx].y = p->y;
			}

			// 자기 자신의 move 응답 + move_time 세팅됨 → latency 측정
			if (ci == my_idx && p->move_time != 0)
			{
				int64_t d_ms = static_cast<int64_t>(NowMs() - p->move_time);
				if (global_delay < d_ms) global_delay++;
				else if (global_delay > d_ms) global_delay--;
			}
		}
		break;
	}

	case PacketType::SC_LOGIN_FAIL:
		// 중복 또는 DB 에러 — 그냥 끊고 재시도 안 함
		DisconnectClient(ci);
		break;

	default:
		// 다른 패킷 (ADD/REMOVE/STAT/CHAT/COMBAT/EFFECT) 무시
		break;
	}
}

// ============================================================
// IOCP 워커
// ============================================================
static void WorkerThread()
{
	while (g_running)
	{
		DWORD io_size = 0;
		ULONG_PTR ci_key = 0;
		OverlappedEx* over = nullptr;

		BOOL ret = GetQueuedCompletionStatus(g_hiocp, &io_size, &ci_key,
		                                     reinterpret_cast<LPOVERLAPPED*>(&over),
		                                     INFINITE);

		if (!g_running) break;

		int ci = static_cast<int>(ci_key);

		if (!ret || io_size == 0)
		{
			if (over && over->event_type == OP_SEND) delete over;
			DisconnectClient(ci);
			continue;
		}

		if (over->event_type == OP_RECV)
		{
			CLIENT& c = g_clients[ci];

			// 누적 버퍼에 append
			if (c.recv_accum_size + (int)io_size <= RECV_ACCUM_CAP)
			{
				memcpy(c.recv_accum + c.recv_accum_size, c.recv_over.iocp_buf, io_size);
				c.recv_accum_size += io_size;
			}
			else
			{
				DisconnectClient(ci);
				continue;
			}

			// 패킷 단위 추출
			while (c.recv_accum_size >= (int)sizeof(PacketHeader))
			{
				const PacketHeader* hdr =
					reinterpret_cast<const PacketHeader*>(c.recv_accum);
				if (c.recv_accum_size < hdr->size) break;

				ProcessPacket(ci, c.recv_accum);

				int remain = c.recv_accum_size - hdr->size;
				memmove(c.recv_accum, c.recv_accum + hdr->size, remain);
				c.recv_accum_size = remain;
			}

			// 다음 recv post
			DWORD recv_flag = 0;
			c.recv_over.wsabuf.buf = reinterpret_cast<CHAR*>(c.recv_over.iocp_buf);
			c.recv_over.wsabuf.len = MAX_BUFF_SIZE;
			int r = WSARecv(c.socket, &c.recv_over.wsabuf, 1, NULL, &recv_flag,
			                &c.recv_over.over, NULL);
			if (r == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
			{
				DisconnectClient(ci);
			}
		}
		else if (over->event_type == OP_SEND)
		{
			delete over;
		}
	}
}

// ============================================================
// 봇 신규 접속 (적응형)
// ============================================================
static void TryAddClient()
{
	static int  delay_multiplier = 1;
	static int  max_limit        = MAX_TEST;
	static bool increasing       = true;

	if (active_clients >= MAX_TEST) return;
	if (num_connections >= MAX_CLIENTS) return;

	auto duration = high_resolution_clock::now() - last_connect_time;
	auto durMs = duration_cast<milliseconds>(duration).count();
	if (ACCEPT_DELAY * delay_multiplier > durMs) return;

	int t_delay = global_delay;

	// 한계 초과 → 봇 줄임
	if (DELAY_LIMIT2 < t_delay)
	{
		if (increasing)
		{
			max_limit  = active_clients;
			increasing = false;
		}
		if (active_clients < 100) return;
		if (ACCEPT_DELAY * 10 > durMs) return;

		last_connect_time = high_resolution_clock::now();
		DisconnectClient(client_to_close);
		client_to_close++;
		return;
	}
	else if (DELAY_LIMIT < t_delay)
	{
		delay_multiplier = 10;
		return;
	}

	if (max_limit - (max_limit / 20) < active_clients) return;

	increasing        = true;
	last_connect_time = high_resolution_clock::now();

	int idx = num_connections;
	g_clients[idx].socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP,
	                                   NULL, 0, WSA_FLAG_OVERLAPPED);

	sockaddr_in serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port   = htons(SERVER_PORT);
	inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

	int result = WSAConnect(g_clients[idx].socket, (sockaddr*)&serverAddr,
	                        sizeof(serverAddr), NULL, NULL, NULL, NULL);
	if (result != 0)
	{
		closesocket(g_clients[idx].socket);
		return;
	}

	g_clients[idx].recv_accum_size = 0;
	ZeroMemory(&g_clients[idx].recv_over, sizeof(g_clients[idx].recv_over));
	g_clients[idx].recv_over.event_type = OP_RECV;
	g_clients[idx].recv_over.wsabuf.buf = reinterpret_cast<CHAR*>(g_clients[idx].recv_over.iocp_buf);
	g_clients[idx].recv_over.wsabuf.len = MAX_BUFF_SIZE;

	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_clients[idx].socket),
	                       g_hiocp, idx, 0);

	// 로그인 패킷
	CS_Login l_packet;
	l_packet.header.size = sizeof(l_packet);
	l_packet.header.type = static_cast<uint16_t>(PacketType::CS_LOGIN);
	sprintf_s(l_packet.name, sizeof(l_packet.name), "bot_%d", idx);
	SendPacket(idx, &l_packet, sizeof(l_packet));

	// 첫 recv 시작
	DWORD recv_flag = 0;
	int r = WSARecv(g_clients[idx].socket, &g_clients[idx].recv_over.wsabuf, 1,
	                NULL, &recv_flag, &g_clients[idx].recv_over.over, NULL);
	if (r == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
	{
		DisconnectClient(idx);
		return;
	}

	num_connections++;
}

// ============================================================
// 봇 행동 루프
// ============================================================
static void TestThread()
{
	while (g_running)
	{
		TryAddClient();

		auto now = high_resolution_clock::now();
		for (int i = 0; i < num_connections; ++i)
		{
			if (!g_clients[i].connected) continue;
			if (g_clients[i].last_move_time + 1s > now) continue;

			g_clients[i].last_move_time = now;

			CS_Move move_pkt;
			move_pkt.header.size = sizeof(move_pkt);
			move_pkt.header.type = static_cast<uint16_t>(PacketType::CS_MOVE);
			move_pkt.direction   = rand() % 4;
			move_pkt.move_time   = NowMs();
			SendPacket(i, &move_pkt, sizeof(move_pkt));
		}

		std::this_thread::sleep_for(milliseconds(20));
	}
}

// ============================================================
// API
// ============================================================
void InitializeNetwork()
{
	for (auto& c : g_clients)
	{
		c.connected       = false;
		c.id              = INVALID_ID;
		c.recv_accum_size = 0;
	}
	for (auto& m : client_map) m = INVALID_ID;

	num_connections   = 0;
	last_connect_time = high_resolution_clock::now();

	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);

	g_hiocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	for (int i = 0; i < 6; ++i)
	{
		worker_threads.emplace_back(WorkerThread);
	}
	test_thread = std::thread(TestThread);
}

void ShutdownNetwork()
{
	g_running = false;
	for (int i = 0; i < num_connections; ++i)
	{
		DisconnectClient(i);
	}
	for (int i = 0; i < (int)worker_threads.size(); ++i)
	{
		PostQueuedCompletionStatus(g_hiocp, 0, 0, NULL);
	}
	if (test_thread.joinable()) test_thread.join();
	for (auto& t : worker_threads)
	{
		if (t.joinable()) t.join();
	}
	CloseHandle(g_hiocp);
	WSACleanup();
}

void GetPointCloud(int* size, float** points)
{
	int idx = 0;
	for (int i = 0; i < num_connections && idx < MAX_TEST; ++i)
	{
		if (g_clients[i].connected)
		{
			point_cloud[idx * 2 + 0] = static_cast<float>(g_clients[i].x);
			point_cloud[idx * 2 + 1] = static_cast<float>(g_clients[i].y);
			idx++;
		}
	}
	*size   = idx;
	*points = point_cloud;
}
