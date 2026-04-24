#pragma once
#include <cstdint>
#include <string>
#include <shared_mutex>
#include "Types.h"

enum class ObjectType : uint8_t
{
	PLAYER = 0,
	MONSTER
};

class GameObject
{
public:
	GameObject(ObjectID id, ObjectType type)
		: m_id(id), m_type(type) {}
	virtual ~GameObject() = default;

	// 복사 / 이동 금지 명시
	GameObject(const GameObject&) = delete;
	GameObject& operator=(const GameObject&) = delete;
	GameObject(GameObject&&) = delete;
	GameObject& operator=(GameObject&&) = delete;

	// Getter
	ObjectID GetId() const { return m_id; }
	ObjectType GetType() const { return m_type; }

	int16_t GetX() const { std::shared_lock lock(m_lock); return m_pos.x; }
	int16_t GetY() const { std::shared_lock lock(m_lock); return m_pos.y; }
	Position GetPos() const { std::shared_lock lock(m_lock); return m_pos; }
	uint16_t GetLevel() const { std::shared_lock lock(m_lock); return m_level; }
	int32_t GetHp() const { std::shared_lock lock(m_lock); return m_hp; }
	int32_t GetMaxHp() const { std::shared_lock lock(m_lock); return m_maxHp; }

	std::string GetName() const {
		std::shared_lock lock(m_lock);
		return m_name;
	}

	// Setter/Modifier
	void SetPos(int16_t x, int16_t y)
	{
		std::unique_lock lock(m_lock);
		m_pos.x = x;
		m_pos.y = y;
	}
	void SetHp(int32_t hp)
	{
		std::unique_lock lock(m_lock);
		m_hp = hp;
	}
	void TakeDamage(int32_t dmg)
	{
		std::unique_lock lock(m_lock);
		m_hp = (dmg >= m_hp) ? 0 : m_hp - dmg;
	}
	bool IsDead() const
	{ 
		std::shared_lock lock(m_lock);
		return m_hp <= 0;
	}

protected:
	const ObjectID m_id;
	const ObjectType m_type;
	Position m_pos{ 0, 0 };
	uint16_t m_level{ 1 };
	int32_t m_hp{ 100 };
	int32_t m_maxHp{ 100 };
	std::string m_name;

	mutable std::shared_mutex m_lock;
};