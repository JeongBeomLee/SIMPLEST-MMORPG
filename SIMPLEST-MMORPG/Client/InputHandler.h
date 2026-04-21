#pragma once
#include <string>
#include <chrono>
#include "Types.h"

class InputHandler
{
public:
	static InputHandler& GetInstance();

	InputHandler(const InputHandler&) = delete;
	InputHandler& operator=(const InputHandler&) = delete;

	// 메인 메뉴 입력
	struct MenuInputResult
	{
		bool enterPressed = false;
		bool escPressed = false;
	};
	MenuInputResult ProcessMainMenuInput(std::wstring& nameInput);

	// 게임 플레이 입력
	struct GameInputResult
	{
		bool hasMove = false;
		Direction moveDir = Direction::UP;
		bool attackPressed = false;
		bool escPressed = false;
	};
	GameInputResult ProcessGameInput();

private:
	InputHandler() = default;
	~InputHandler() = default;

	// 쿨다운 헬퍼
	bool CheckMoveCooldown();
	bool CheckAttackCooldown();

private:
	std::chrono::steady_clock::time_point m_lastMoveTime{};
	std::chrono::steady_clock::time_point m_lastAttackTime{};
};

