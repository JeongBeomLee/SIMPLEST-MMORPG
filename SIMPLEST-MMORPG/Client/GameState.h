#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <string>
#include "Map.h"
#include "Types.h"

struct RemoteObject
{
	ObjectID id;
	int16_t x, y;
	uint8_t type;
	uint16_t level;
	int32_t hp, maxHp;
	std::string name;
};

struct MyPlayer
{
	ObjectID id;
	int16_t x, y;
	uint16_t level;
	int32_t exp;
	int32_t hp, maxHp;
	std::string name;
};

struct AttackEffect
{
	int16_t x, y;
	std::chrono::steady_clock::time_point startTime;
};

class GameState
{
public:
	static GameState& GetInstance();

	GameState(const GameState&) = delete;
	GameState& operator=(const GameState&) = delete;

	bool Init();
	const Map& GetMap() const;

	// Login OK 처리 시 호출
	void SetMyPlayer(const MyPlayer& me);

	// 플레이어 이름 등록
	void SetMyName(const std::string& name);

	// 오브젝트 추가/갱신/제거 (SC_AddObject, SC_MoveObject, SC_RemoveObject)
	void AddObject(const RemoteObject& obj);
	void MoveObject(ObjectID id, int16_t x, int16_t y);
	void RemoveObject(ObjectID id);

	// 내 위치 업데이트 (내 이동 패킷 보낸 뒤 반영)
	void MoveMyPlayer(int16_t x, int16_t y);

	// 조회 (Renderer 가 const 로 접근)
	MyPlayer GetMyPlayer() const;
	std::vector<RemoteObject> GetAllObjects() const;
	std::string GetObjectName(ObjectID id) const;

	bool IsLoggedIn() const { return m_loggedIn; }

	void AddAttackEffect(int16_t x, int16_t y);
	std::vector<AttackEffect> GetActiveEffects();

private:
	GameState() = default;
	~GameState() = default;

private:
	Map m_map;
	MyPlayer m_me{};
	bool m_loggedIn{ false };

	std::unordered_map<ObjectID, RemoteObject> m_objects;
	std::vector<AttackEffect> m_effects;

	mutable std::mutex m_mutex;
};

