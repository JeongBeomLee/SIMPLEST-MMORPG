#include "InputHandler.h"
#include "Constants.h"
#include <conio.h>

InputHandler& InputHandler::GetInstance()
{
	static InputHandler instance;
	return instance;
}

InputHandler::MenuInputResult InputHandler::ProcessMainMenuInput(std::wstring& nameInput)
{
	MenuInputResult result;
	while (_kbhit()) 
	{
		int ch = _getch();

		if (ch == 27) 
		{ 
			result.escPressed = true; 
			break;
		}

		if (ch == 13) 
		{ 
			result.enterPressed = true; 
			break; 
		}

		if (ch == 8) 
		{
			if (!nameInput.empty())
			{
				nameInput.pop_back();
			}
			continue;
		}

		// 특수 키 prefix 무시
		if (ch == 0 || ch == 0xE0) 
		{ 
			_getch(); 
			continue; 
		}

		// 영문/숫자만 허용, 16자 제한
		if (nameInput.size() < 16 &&
			((ch >= 'a' && ch <= 'z') ||
			 (ch >= 'A' && ch <= 'Z') ||
			 (ch >= '0' && ch <= '9')))
		{
			nameInput.push_back(static_cast<wchar_t>(ch));
		}
	}
	return result;
}

InputHandler::GameInputResult InputHandler::ProcessGameInput()
{
	GameInputResult result;
	while (_kbhit()) 
	{
		int ch = _getch();

		if (ch == 27) 
		{ 
			result.escPressed = true; 
			continue; 
		}

		// 화살표
		if (ch == 0 || ch == 0xE0) 
		{
			int arrow = _getch();
			Direction dir;
			bool isMove = true;
			switch (arrow)
			{
			case 72:
				dir = Direction::UP;
				break;
			case 80: 
				dir = Direction::DOWN;
				break;
			case 75:
				dir = Direction::LEFT;
				break;
			case 77:
				dir = Direction::RIGHT;
				break;
			default:
				isMove = false;
			}

			if (isMove && CheckMoveCooldown()) 
			{
				result.hasMove = true;
				result.moveDir = dir;
			}
			continue;
		}

		if (ch == ' ' && CheckAttackCooldown())
		{
			result.attackPressed = true;
		}
	}
	return result;
}

bool InputHandler::CheckMoveCooldown()
{
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastMoveTime).count();
	if (elapsed < MOVE_COOLDOWN_MS)
	{
		return false;
	}
	m_lastMoveTime = now;
	return true;
}

bool InputHandler::CheckAttackCooldown()
{
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastAttackTime).count();
	if (elapsed < ATTACK_COOLDOWN_MS)
	{
		return false;
	}
	m_lastAttackTime = now;
	return true;
}
