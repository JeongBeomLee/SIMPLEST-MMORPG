#include "NetworkClient.h"
#include "Protocol.h"
#include "Logger.h"
#include "GameState.h"
#include "Renderer.h"

#pragma comment(lib, "ws2_32.lib")

NetworkClient::NetworkClient()
	: m_socket(INVALID_SOCKET)
	, m_running(false)
{
}

NetworkClient::~NetworkClient()
{
	Disconnect();
}

bool NetworkClient::Connect(const char* ip, uint16_t port)
{
	// 1. Winsock 초기화
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		LOG("WSAStartup failed");
		return false;
	}

	// 2. 소켓 생성
	m_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_socket == INVALID_SOCKET)
	{
		LOG("socket failed");
		return false;
	}

	// 3. 서버 주소 설정
	sockaddr_in serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &serverAddr.sin_addr);

	// 4. 연결
	if (connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		LOG("connect failed");
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
		return false;
	}

	// 5. 수신 스레드 시작
	m_running = true;
	m_recvThread = std::thread(&NetworkClient::RecvThread, this);

	LOG("Connected to " << ip << ":" << port);
	return true;
}

void NetworkClient::Disconnect()
{
	if (!m_running)
	{
		return;
	}

	m_running = false;

	if (m_socket != INVALID_SOCKET)
	{
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}

	if (m_recvThread.joinable())
	{
		m_recvThread.join();
	}

	WSACleanup();
}

void NetworkClient::SendPacket(const void* data, uint16_t size)
{
	int sent = 0;
	const char* ptr = static_cast<const char*>(data);

	while (sent < size)
	{
		int n = send(m_socket, ptr + sent, size - sent, 0);
		if (n <= 0)
		{
			LOG("send failed");
			return;
		}
		sent += n;
	}
}

void NetworkClient::RecvThread()
{
	while (m_running)
	{
		// 1. RingBuffer 빈 공간에 직접 recv
		char* writePtr = m_recvBuffer.GetWritePtr();
		int writeSize = m_recvBuffer.GetContiguousWriteSize();

		if (writeSize == 0)
		{
			// 버퍼 꽉 참 — 비정상 (패킷 파싱이 밀림)
			LOG("recv buffer full");
			break;
		}

		int bytes = recv(m_socket, writePtr, writeSize, 0);
		if (bytes <= 0)
		{
			// 연결 끊김 또는 에러
			break;
		}

		m_recvBuffer.OnWrite(bytes);

		// 2. 패킷 추출 루프
		while (m_recvBuffer.GetUsedSize() >= sizeof(PacketHeader))
		{
			PacketHeader header;
			m_recvBuffer.Peek(reinterpret_cast<char*>(&header), sizeof(PacketHeader));

			if (m_recvBuffer.GetUsedSize() < header.size)
			{
				break;
			}

			char packetBuf[512];
			m_recvBuffer.Peek(packetBuf, header.size);
			m_recvBuffer.Pop(header.size);

			OnPacket(packetBuf, header.size);
		}
	}

	LOG("RecvThread terminated");
}

void NetworkClient::OnPacket(const char* data, uint16_t size)
{
	const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);
	GameState& state = GameState::GetInstance();

	switch (static_cast<PacketType>(header->type))
	{
	case PacketType::SC_LOGIN_OK:
	{
		const SC_LoginOk* p = reinterpret_cast<const SC_LoginOk*>(data);
		MyPlayer me = state.GetMyPlayer();
		me.id = p->my_id;
		me.x = p->x;
		me.y = p->y;
		me.level = p->level;
		me.exp = p->exp;
		me.hp = p->hp;
		me.maxHp = p->max_hp;
		state.SetMyPlayer(me);
		LOG("LOGIN_OK: id=" << me.id << " pos(" << me.x << "," << me.y << ")");
		break;
	}
	case PacketType::SC_LOGIN_FAIL:
	{
		LOG("LOGIN_FAIL received");
		break;
	}
	case PacketType::SC_ADD_OBJECT:
	{
		const SC_AddObject* p = reinterpret_cast<const SC_AddObject*>(data);
		RemoteObject obj;
		obj.id = p->object_id;
		obj.x = p->x;
		obj.y = p->y;
		obj.type = p->object_type;
		obj.level = p->level;
		obj.hp = p->hp;
		obj.maxHp = p->max_hp;
		obj.name = std::string(p->name);
		state.AddObject(obj);
		LOG("ADD_OBJECT: id=" << obj.id << " name=" << obj.name << " pos=(" << obj.x << "," << obj.y << ")");
		break;
	}
	case PacketType::SC_REMOVE_OBJECT:
	{
		const SC_RemoveObject* p = reinterpret_cast<const SC_RemoveObject*>(data);
		state.RemoveObject(p->object_id);
		LOG("REMOVE_OBJECT: id=" << p->object_id);
		break;
	}
	case PacketType::SC_MOVE_OBJECT:
	{
		const SC_MoveObject* p = reinterpret_cast<const SC_MoveObject*>(data);
		if (p->object_id == state.GetMyPlayer().id)
		{
			// 자기 자신 강제 동기화 (사망 후 리스폰 등)
			state.MoveMyPlayer(p->x, p->y);
		}
		else
		{
			state.MoveObject(p->object_id, p->x, p->y);
		}
		break;
	}
	case PacketType::SC_STAT_CHANGE:
	{
		const SC_StatChange* p = reinterpret_cast<const SC_StatChange*>(data);

		// 내 ID 면 HUD 갱신, 다른 사람이면 GameState 의 RemoteObject 갱신
		if (p->object_id == GameState::GetInstance().GetMyPlayer().id)
		{
			MyPlayer me = GameState::GetInstance().GetMyPlayer();
			me.hp = p->hp;
			me.maxHp = p->max_hp;
			me.exp = p->exp;
			me.level = p->level;
			GameState::GetInstance().SetMyPlayer(me);
		}
		break;
	}
	case PacketType::SC_COMBAT_MESSAGE:
	{
		const SC_CombatMessage* p = reinterpret_cast<const SC_CombatMessage*>(data);

		GameState& gameState = GameState::GetInstance();
		std::string attackerName = gameState.GetObjectName(p->attacker_id);
		std::string targetName = gameState.GetObjectName(p->target_id);

		std::wstring atkW = LogInternal::AsciiToWString(attackerName);
		std::wstring tgtW = LogInternal::AsciiToWString(targetName);

		wchar_t buf[128];
		swprintf_s(buf, L"[Combat] %ls -> %ls : -%d HP", atkW.c_str(), tgtW.c_str(), p->damage);

		WORD combatColor = FOREGROUND_RED | FOREGROUND_GREEN;
		Renderer::GetInstance().PushLog(buf, combatColor);
		break;
	}
	case PacketType::SC_ATTACK_EFFECT:
	{
		const SC_AttackEffect* p = reinterpret_cast<const SC_AttackEffect*>(data);
		GameState::GetInstance().AddAttackEffect(p->x, p->y);
		break;
	}
	default:
		LOG("Unknown packet type: " << header->type);
		break;
	}
}
