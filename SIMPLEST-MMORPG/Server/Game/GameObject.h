#pragma once
#include <cstdint>
#include <string>
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

	// Getter
	ObjectID GetId() const { return m_id; }
	ObjectType GetType() const { return m_type; }
	int16_t GetX() const { return m_pos.x; }
	int16_t GetY() const { return m_pos.y; }
	Position GetPos() const { return m_pos; }
	uint16_t GetLevel() const { return m_level; }
	int32_t GetHp() const { return m_hp; }
	int32_t GetMaxHp() const { return m_maxHp; }
	const std::string& GetName() const { return m_name; }

	// Setter/Modifier
	void SetPos(int16_t x, int16_t y) { m_pos.x = x; m_pos.y = y; }
	void SetHp(int32_t hp) { m_hp = hp; }
	void TakeDamage(int32_t dmg) { m_hp = (dmg >= m_hp) ? 0 : m_hp - dmg; }
	bool IsDead() const { return m_hp <= 0; }

protected:
	ObjectID m_id;
	ObjectType m_type;
	Position m_pos{ 0, 0 };
	uint16_t m_level{ 1 };
	int32_t m_hp{ 100 };
	int32_t m_maxHp{ 100 };
	std::string m_name;
};