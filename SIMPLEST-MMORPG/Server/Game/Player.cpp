#include "Player.h"
#include "Constants.h"
#include <algorithm>

Player::Player(ObjectID id, Session* session, const std::string& name)
	: GameObject(id, ObjectType::PLAYER)
	, m_session(session)
{
	m_name = name;
}

void Player::GainExp(int32_t amount)
{
	std::unique_lock lock(m_lock);
	m_exp += amount;

	// LevelUp 인라인 변경 (락 재진입 방지)
	while (m_exp >= 100 * (1 << (m_level - 1)) && m_level < MAX_LEVEL)
	{
		m_exp -= 100 * (1 << (m_level - 1));
		m_level++;
		m_maxHp = 100 + (m_level - 1) * 50;
		m_hp = m_maxHp;
	}

	// 만렙 도달 시 경험치 0으로 고정
	if (m_level >= MAX_LEVEL)
	{
		m_exp = 0;
	}
}

int32_t Player::GetLevelUpExp() const
{
	std::shared_lock lock(m_lock);
	return 100 * (1 << (m_level - 1));
}

void Player::Die()
{
	std::unique_lock lock(m_lock);
	m_exp = static_cast<int32_t>(m_exp * (1.0f - DEATH_EXP_PENALTY));
	m_pos = { 0, 0 };
	m_hp = m_maxHp;
}

void Player::RegenHP()
{
	std::unique_lock lock(m_lock);
	int32_t regen = static_cast<int32_t>(m_maxHp * HP_REGEN_RATE);
	m_hp = std::min(m_hp + regen, m_maxHp);
}

bool Player::TryStartRegen()
{
	bool expected = false;
	return m_regenActive.compare_exchange_strong(expected, true);
}

void Player::StopRegen()
{
	m_regenActive.store(false);
}

bool Player::CanMove() const
{
	std::shared_lock lock(m_lock);
	auto now = std::chrono::steady_clock::now();
	auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastMoveTime).count();
	return diff >= MOVE_COOLDOWN_MS;
}

bool Player::CanAttack() const
{
	std::shared_lock lock(m_lock);
	auto now = std::chrono::steady_clock::now();
	auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastAttackTime).count();
	return diff >= ATTACK_COOLDOWN_MS;
}

void Player::OnMoved()
{
	std::unique_lock lock(m_lock);
	m_lastMoveTime = std::chrono::steady_clock::now();
}

void Player::OnAttackPerformed()
{
	std::unique_lock lock(m_lock);
	m_lastAttackTime = std::chrono::steady_clock::now();
}
