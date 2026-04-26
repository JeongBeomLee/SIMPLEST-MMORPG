#pragma once
#include "GameObject.h"
#include <cstdint>
#include <chrono>
#include <shared_mutex>
#include <atomic>

class Session;

class Player : public GameObject
{
public:
	Player(ObjectID id, Session* session, const std::string& name, int64_t dbId);
	~Player() override = default;

	Session* GetSession() const { return m_session; }

	// 경험치/레벨
	int32_t GetExp() const { std::shared_lock lock(m_lock); return m_exp; }
	void GainExp(int32_t amount);
	int32_t GetLevelUpExp() const;

	// DB
	void SetExp(int32_t exp);
	int64_t GetDbId() const { return m_dbId; }

	// 전투
	void Die();
	void RegenHP();

	// HP 회복 타이머
	bool TryStartRegen();
	void StopRegen();

	// 쿨다운
	bool CanMove() const;
	bool CanAttack() const;
	bool CanChat() const;
	void OnMoved();
	void OnAttackPerformed();
	void OnChatPerformed();

private:
	Session* const m_session;
	const int64_t m_dbId{ 0 };
	int32_t m_exp{ 0 };

	std::chrono::steady_clock::time_point m_lastMoveTime{};
	std::chrono::steady_clock::time_point m_lastAttackTime{};
	std::chrono::steady_clock::time_point m_lastChatTime{};

	std::atomic<bool> m_regenActive{ false };
};

