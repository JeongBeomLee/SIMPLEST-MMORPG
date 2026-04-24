#pragma once
#include "GameObject.h"
#include <cstdint>
#include <chrono>
#include <shared_mutex>

class Session;

class Player : public GameObject
{
public:
	Player(ObjectID id, Session* session, const std::string& name);
	~Player() override = default;

	Session* GetSession() const { return m_session; }

	// 경험치/레벨
	int32_t GetExp() const { std::shared_lock lock(m_lock); return m_exp; }
	void GainExp(int32_t amount);
	int32_t GetLevelUpExp() const;

	// 전투
	void Die();
	void RegenHP();

	// 쿨다운
	bool CanMove() const;
	bool CanAttack() const;
	void OnMoved();
	void OnAttackPerformed();

private:
	Session* const m_session;
	int32_t m_exp{ 0 };

	std::chrono::steady_clock::time_point m_lastMoveTime{};
	std::chrono::steady_clock::time_point m_lastAttackTime{};
};

