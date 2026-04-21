#include "Renderer.h"
#include "GameState.h"
#include <cwchar>

namespace 
{
	// 뷰포트
	constexpr int VIEWPORT_TILES = 27;
	constexpr int TILE_WIDTH = 2; // 한 타일 = 2 열 (문자 + 공백)
	constexpr int VIEWPORT_BORDER_X = 0;
	constexpr int VIEWPORT_BORDER_Y = 0;
	constexpr int VIEWPORT_BORDER_W = VIEWPORT_TILES * TILE_WIDTH + 3; // 57
	constexpr int VIEWPORT_BORDER_H = VIEWPORT_TILES + 2; // 29
	constexpr int TILE_OFFSET_X = 2; // 왼쪽 테두리 + 왼쪽 패딩
	constexpr int TILE_OFFSET_Y = 1;

	constexpr int CENTER_TILE_X = 13;
	constexpr int CENTER_TILE_Y = 13;

	// HUD (오른쪽 위) — 뷰포트 우측(57) + 1칸 여백 후 시작
	constexpr int HUD_X = 58;
	constexpr int HUD_Y = 0;
	constexpr int HUD_W = 62;
	constexpr int HUD_H = 12;

	// LOG (오른쪽 아래) — 뷰포트 하단 테두리(행 28)와 정렬
	constexpr int LOG_X = 58;
	constexpr int LOG_Y = 12;
	constexpr int LOG_W = 62;
	constexpr int LOG_H = 17;
}

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

void Renderer::DrawTile(int tileX, int tileY, wchar_t ch, WORD attr)
{
	int col = TILE_OFFSET_X + tileX * TILE_WIDTH;
	int row = TILE_OFFSET_Y + tileY;
	SetChar(col, row, ch, attr);
	SetChar(col + 1, row, L' ', attr);
}

void Renderer::RenderMainMenu(const std::wstring& nameInput)
{
	Clear();

	// 전체 테두리
	DrawBorder(0, 0, SCREEN_W, SCREEN_H);

	// 타이틀 (아스키 아트로 돌아가게 만들고 싶음)
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

	// 커서 표시 (깜빡이게 해야함)
	int cursorX = boxX + 2 + (int)min(nameInput.size(), size_t(16));
	SetChar(cursorX, promptY, L'_', FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);

	// 안내 텍스트
	DrawString(48, 17, L"[ ENTER ] to login", FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	DrawString(49, 18, L"[ ESC ] to quit", FOREGROUND_RED | FOREGROUND_INTENSITY);

	Flush();
}

void Renderer::RenderGame() 
{
	if (!GameState::GetInstance().IsLoggedIn())
	{
		return;
	}

	Clear();
	DrawViewport();
	DrawHUD();
	DrawLogBox();
	Flush();
}

void Renderer::PushLog(const std::wstring& line) 
{
	std::lock_guard lock(m_logMutex);
	m_logLines.push_back(line);
	while (m_logLines.size() > MAX_LOG_LINES)
	{
		m_logLines.pop_front();
	}
}

void Renderer::DrawViewport()
{
	GameState& state = GameState::GetInstance();
	MyPlayer me = state.GetMyPlayer();
	auto objects = state.GetAllObjects();

	WORD emptyColor = FOREGROUND_INTENSITY;

	// 타일 영역 외부를 감싸는 뷰포트 테두리
	DrawBorder(VIEWPORT_BORDER_X, VIEWPORT_BORDER_Y, VIEWPORT_BORDER_W, VIEWPORT_BORDER_H);

	// 빈 타일
	for (int ty = 0; ty < VIEWPORT_TILES; ++ty)
	{
		for (int tx = 0; tx < VIEWPORT_TILES; ++tx)
		{
			DrawTile(tx, ty, L'.', emptyColor);
		}
	}

	// 다른 오브젝트
	for (const auto& obj : objects)
	{
		int tileX = obj.x - me.x + CENTER_TILE_X;
		int tileY = obj.y - me.y + CENTER_TILE_Y;

		if (tileX < 0 || tileX >= VIEWPORT_TILES || tileY < 0 || tileY >= VIEWPORT_TILES)
		{
			continue;
		}

		wchar_t ch;
		WORD color;
		if (obj.type == 0)
		{
			ch = L'P';
			color = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		}
		else
		{
			ch = L'M';
			color = FOREGROUND_RED | FOREGROUND_INTENSITY;
		}

		DrawTile(tileX, tileY, ch, color);
	}

	// 내 플레이어 (항상 중앙)
	WORD myColor = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
	DrawTile(CENTER_TILE_X, CENTER_TILE_Y, L'@', myColor);
}

void Renderer::DrawHUD()
{
	GameState& state = GameState::GetInstance();
	MyPlayer me = state.GetMyPlayer();

	// 테두리
	DrawBorder(HUD_X, HUD_Y, HUD_W, HUD_H);

	WORD titleColor = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
	WORD valueColor = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;

	wchar_t buf[64];

	// Line 1: Lv
	swprintf_s(buf, L"Lv %d", me.level);
	DrawString(HUD_X + 2, HUD_Y + 1, L"Level:", titleColor);
	DrawString(HUD_X + 10, HUD_Y + 1, buf, valueColor);

	// Line 2: HP
	swprintf_s(buf, L"%d / %d", me.hp, me.maxHp);
	DrawString(HUD_X + 2, HUD_Y + 3, L"HP:", titleColor);
	DrawString(HUD_X + 10, HUD_Y + 3, buf, FOREGROUND_RED | FOREGROUND_INTENSITY);

	// Line 3: Exp
	swprintf_s(buf, L"%d", me.exp);
	DrawString(HUD_X + 2, HUD_Y + 5, L"Exp:", titleColor);
	DrawString(HUD_X + 10, HUD_Y + 5, buf, valueColor);

	// Line 4: Pos
	swprintf_s(buf, L"(%d, %d)", me.x, me.y);
	DrawString(HUD_X + 2, HUD_Y + 7, L"Pos:", titleColor);
	DrawString(HUD_X + 10, HUD_Y + 7, buf, valueColor);
}

void Renderer::DrawLogBox()
{
	// 테두리
	DrawBorder(LOG_X, LOG_Y, LOG_W, LOG_H);

	// 타이틀
	DrawString(LOG_X + 2, LOG_Y, L" [ LOG / CHAT ] ", FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);

	// 로그 줄 출력
	std::lock_guard<std::mutex> lock(m_logMutex);

	int maxContentH = LOG_H - 2; // 테두리 제외
	int startRow = LOG_Y + 1;

	int i = 0;
	for (const auto& line : m_logLines)
	{
		if (i >= maxContentH)
		{
			break;
		}

		int maxLen = LOG_W - 2;
		std::wstring truncated = line.substr(0, maxLen);

		DrawString(LOG_X + 1, startRow + i, truncated.c_str(), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		++i;
	}
}
