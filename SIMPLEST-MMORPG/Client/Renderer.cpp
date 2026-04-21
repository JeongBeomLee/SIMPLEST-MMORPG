#include "Renderer.h"
#include <cwchar>

Renderer& Renderer::GetInstance()
{
	static Renderer instance;
	return instance;
}

bool Renderer::Init()
{
	m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if (m_hConsole == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	
	// 버퍼 크기 설정
	COORD bufferSize = { SCREEN_W, SCREEN_H };
	SetConsoleScreenBufferSize(m_hConsole, bufferSize);

	// 콘솔 크기 고정
	SMALL_RECT windowSize = { 0, 0, SCREEN_W - 1, SCREEN_H - 1 };
	SetConsoleWindowInfo(m_hConsole, TRUE, &windowSize);

	// 커서 숨기기
	CONSOLE_CURSOR_INFO cursorInfo;
	GetConsoleCursorInfo(m_hConsole, &cursorInfo);
	cursorInfo.bVisible = FALSE;
	SetConsoleCursorInfo(m_hConsole, &cursorInfo);

	// 콘솔 제목 설정
	SetConsoleTitleW(L"SIMPLEST MMORPG");

	return true;
}

void Renderer::Clear()
{
	for (int i = 0; i < SCREEN_W * SCREEN_H; ++i)
	{
		m_buffer[i].Char.UnicodeChar = L' ';
		m_buffer[i].Attributes = 0;
	}
}

void Renderer::Flush()
{
	COORD bufferSize = { SCREEN_W, SCREEN_H };
	COORD bufferCoord = { 0, 0 };
	SMALL_RECT writeRegion = { 0, 0, SCREEN_W - 1, SCREEN_H - 1 };
	WriteConsoleOutputW(m_hConsole, m_buffer, bufferSize, bufferCoord, &writeRegion);
}

void Renderer::SetChar(int x, int y, wchar_t ch, WORD attr)
{
	if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H)
	{
		return;
	}
	CHAR_INFO& ci = m_buffer[y * SCREEN_W + x];
	ci.Char.UnicodeChar = ch;
	ci.Attributes = attr;
}

void Renderer::DrawString(int x, int y, const wchar_t* str, WORD attr)
{
	for (int i = 0; str[i] != L'\0'; ++i)
	{
		SetChar(x + i, y, str[i], attr);
	}
}

void Renderer::DrawBorder(int x, int y, int w, int h)
{
	WORD attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; // 노란색

	// 모서리
	SetChar(x, y, L'+', attr);
	SetChar(x + w - 1, y, L'+', attr);
	SetChar(x, y + h - 1, L'+', attr);
	SetChar(x + w - 1, y + h - 1, L'+', attr);

	// 위/아래 가로줄
	for (int i = 1; i < w - 1; ++i) {
		SetChar(x + i, y, L'-', attr);
		SetChar(x + i, y + h - 1, L'-', attr);
	}
	// 왼/오른 세로줄
	for (int j = 1; j < h - 1; ++j) {
		SetChar(x, y + j, L'|', attr);
		SetChar(x + w - 1, y + j, L'|', attr);
	}
}

void Renderer::RenderMainMenu(const std::wstring& nameInput)
{
	Clear();

	// 전체 테두리
	DrawBorder(0, 0, SCREEN_W, SCREEN_H);

	// 타이틀
	const wchar_t* title = L"S I M P L E S T   M M O R P G";
	int titleLen = (int)wcslen(title);
	int titleX = (SCREEN_W - titleLen) / 2;
	DrawString(titleX, 8, title, FOREGROUND_GREEN | FOREGROUND_INTENSITY);

	DrawString(45, 10, L"( Portfolio Project )", FOREGROUND_INTENSITY);

	// 입력 프롬프트
	const wchar_t* prompt = L"Enter your name: ";
	int promptX = 40;
	int promptY = 14;
	DrawString(promptX, promptY, prompt, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

	// 입력 박스: [ ______________ ]
	int boxX = promptX + (int)wcslen(prompt);
	DrawString(boxX, promptY, L"[                  ]", FOREGROUND_INTENSITY);

	// 입력된 이름 표시 (박스 안)
	for (size_t i = 0; i < nameInput.size() && i < 16; ++i)
	{
		SetChar(boxX + 2 + (int)i, promptY, nameInput[i], FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	}

	// 커서 표시
	int cursorX = boxX + 2 + (int)min(nameInput.size(), size_t(16));
	SetChar(cursorX, promptY, L'_', FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);

	// 안내 텍스트
	DrawString(48, 17, L"[ ENTER ] to login", FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	DrawString(49, 18, L"[ ESC ] to quit", FOREGROUND_RED | FOREGROUND_INTENSITY);

	Flush();
}

void Renderer::RenderGame() 
{
}

void Renderer::PushLog(const std::wstring&) 
{
}

void Renderer::DrawViewport() 
{
}

void Renderer::DrawHUD() 
{
}

void Renderer::DrawLogBox() 
{
}
