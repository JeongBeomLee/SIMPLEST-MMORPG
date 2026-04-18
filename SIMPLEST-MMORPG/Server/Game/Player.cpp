#include "Player.h"
#include "Constants.h"
#include <algorithm>

Player::Player(ObjectID id, Session* session)
	: GameObject(id, ObjectType::PLAYER)
	, m_session(session)
{
}

void Player::GainExp(int32_t amount)
{
	m_exp += amount;

	while (m_exp >= GetLevelUpExp() && m_level < MAX_LEVEL)
	{
		m_exp -= GetLevelUpExp();
		LevelUp();
	}

	// 만렙 도달 시 경험치 0으로 고정
	if (m_level >= MAX_LEVEL)
	{
		m_exp = 0;
	}
}

int32_t Player::GetLevelUpExp() const
{
	return 100 * (1 << (m_level - 1));
}

void Player::LevelUp()
{
	if (m_level >= MAX_LEVEL)
	{
		return;
	}

	m_level++;
	m_maxHp = 100 + (m_level - 1) * 50;
	m_hp = m_maxHp;
}

void Player::Die()
{
	m_exp = static_cast<int32_t>(m_exp * (1.0f - DEATH_EXP_PENALTY));
	m_pos = { 0, 0 };
	m_hp = m_maxHp;
}

void Player::RegenHP()
{
	int32_t regen = static_cast<int32_t>(m_maxHp * HP_REGEN_RATE);
	m_hp = std::min(m_hp + regen, m_maxHp);
}

bool Player::CanMove() const
{
	auto now = std::chrono::steady_clock::now();
	auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastMoveTime).count();
	return diff >= MOVE_COOLDOWN_MS;
}

bool Player::CanAttack() const
{
	auto now = std::chrono::steady_clock::now();
	auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastAttackTime).count();
	return diff >= ATTACK_COOLDOWN_MS;
}

void Player::OnMoved()
{
	m_lastMoveTime = std::chrono::steady_clock::now();
}

void Player::OnAttackPerformed()
{
	m_lastAttackTime = std::chrono::steady_clock::now();
}
