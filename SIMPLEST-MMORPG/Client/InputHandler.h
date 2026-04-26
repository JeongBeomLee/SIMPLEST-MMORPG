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

	// 채팅 모드 관리
	bool IsChatMode() const { return m_isChatMode; }
	void EnterChatMode() { m_isChatMode = true; }
	void ExitChatMode() { m_isChatMode = false; }

	struct ChatInputResult
	{
		bool submit = false;
		bool cancel = false;
		bool channelToggled = false;
	};
	ChatInputResult ProcessChatInput(std::wstring& message);

	// 게임 플레이 입력
	struct GameInputResult
	{
		bool hasMove = false;
		Direction moveDir = Direction::UP;
		bool attackPressed = false;
		bool escPressed = false;
		bool chatModeEntered = false;
	};
	GameInputResult ProcessGameInput();

private:
	InputHandler() = default;
	~InputHandler() = default;

	// 쿨다운 헬퍼
	bool CheckMoveCooldown();
	bool CheckAttackCooldown();

private:
	bool m_isChatMode = false;

	std::chrono::steady_clock::time_point m_lastMoveTime{};
	std::chrono::steady_clock::time_point m_lastAttackTime{};
};

