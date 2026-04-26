#include "Renderer.h"
#include "GameState.h"
#include "Constants.h"
#include <cwchar>
#include <cmath>
#include <cstdlib>
#include <ctime>

namespace 
{
	// 뷰포트
	constexpr int VIEWPORT_TILES = 21;
	constexpr int TILE_WIDTH = 2;

	// 고정 테두리 크기
	constexpr int VIEWPORT_BORDER_X = 0;
	constexpr int VIEWPORT_BORDER_Y = 0;
	constexpr int VIEWPORT_BORDER_W = 57;
	constexpr int VIEWPORT_BORDER_H = 29;

	// 타일 영역 + 패딩
	constexpr int TILE_AREA_W = VIEWPORT_TILES * TILE_WIDTH;
	constexpr int TILE_AREA_H = VIEWPORT_TILES;
	constexpr int TILE_OFFSET_X = 1 + ((VIEWPORT_BORDER_W - 2) - TILE_AREA_W + 1) / 2;
	constexpr int TILE_OFFSET_Y = 1 + ((VIEWPORT_BORDER_H - 2) - TILE_AREA_H + 1) / 2;

	// 센터
	constexpr int CENTER_TILE_X = VIEWPORT_TILES / 2;
	constexpr int CENTER_TILE_Y = VIEWPORT_TILES / 2;

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

	// 블록 문자 폰트 정의 (5행 고정)
	constexpr int FONT_ROWS = 5;

	struct LetterDef
	{
		int width;
		const wchar_t* rows[FONT_ROWS];
	};

	const LetterDef FONT[] =
	{
		{ 5, { L" ███ ", L"█    ", L" ███ ", L"    █", L" ███ " } },  // S
		{ 3, { L"███",   L" █ ",   L" █ ",   L" █ ",   L"███"   } },  // I
		{ 5, { L"█   █", L"██ ██", L"█ █ █", L"█   █", L"█   █" } },  // M
		{ 5, { L"████ ", L"█   █", L"████ ", L"█    ", L"█    " } },  // P
		{ 5, { L"█    ", L"█    ", L"█    ", L"█    ", L"█████" } },  // L
		{ 5, { L"█████", L"█    ", L"████ ", L"█    ", L"█████" } },  // E
		{ 5, { L"█████", L"  █  ", L"  █  ", L"  █  ", L"  █  " } },  // T
		{ 5, { L" ███ ", L"█   █", L"█   █", L"█   █", L" ███ " } },  // O
		{ 5, { L"████ ", L"█   █", L"████ ", L"█  █ ", L"█   █" } },  // R
		{ 5, { L" ████", L"█    ", L"█  ██", L"█   █", L" ███ " } },  // G
	};

	const int WORD1[] = { 0, 1, 2, 3, 4, 5, 0, 6 };
	constexpr int WORD1_LEN = 8;

	const int WORD2[] = { 2, 2, 7, 8, 3, 9 };
	constexpr int WORD2_LEN = 6;

	constexpr int LETTER_GAP = 2;

	const WORD RAINBOW_COLORS[] =
	{
		FOREGROUND_RED | FOREGROUND_INTENSITY,
		FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
		FOREGROUND_GREEN | FOREGROUND_INTENSITY,
		FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
		FOREGROUND_BLUE | FOREGROUND_INTENSITY,
		FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
	};
	constexpr int NUM_COLORS = 6;

	// 몬스터 이름 → (모양, 기본 색상) 매핑
	struct MonsterVisual
	{
		wchar_t shape;
		WORD baseColor;
	};

	MonsterVisual GetMonsterVisual(const std::string& name)
	{
		if (name == "Pawn")    return { L'♟', FOREGROUND_GREEN | FOREGROUND_INTENSITY };
		if (name == "Knight")  return { L'♞', FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY };
		if (name == "Rook")    return { L'♜', FOREGROUND_RED | FOREGROUND_GREEN };
		if (name == "Bishop")  return { L'♝', FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY };
		if (name == "Queen")   return { L'♛', FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE };

		// 알 수 없는 이름 → 기본 M 빨강 (fallback)
		return { L'M', FOREGROUND_RED | FOREGROUND_INTENSITY };
	}
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

void Renderer::InitStars()
{
	srand((unsigned)time(nullptr));
	const wchar_t starChars[] = { L'.', L'*', L'+', L'\'' };
	const WORD starColors[] =
	{
		FOREGROUND_INTENSITY,
		FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
		FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
	};

	for (int i = 0; i < MAX_STARS; ++i)
	{
		m_stars[i].x = rand() % (SCREEN_W - 2) + 1;
		m_stars[i].y = rand() % (SCREEN_H - 2) + 1;
		m_stars[i].speed = rand() % 3 + 1;  // 1~3 프레임당 1칸
		m_stars[i].ticker = 0;
		m_stars[i].ch = starChars[rand() % 4];
		m_stars[i].color = starColors[rand() % 3];
	}
	m_starsInited = true;
}

void Renderer::UpdateAndDrawStars()
{
	const wchar_t starChars[] = { L'.', L'*', L'+', L'\'' };
	const WORD starColors[] = {
		FOREGROUND_INTENSITY,
		FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
		FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
	};

	for (int i = 0; i < MAX_STARS; ++i)
	{
		Star& s = m_stars[i];

		// 낙하 업데이트
		++s.ticker;
		if (s.ticker >= s.speed)
		{
			s.ticker = 0;
			++s.y;

			// 화면 밖으로 나가면 맨 위에서 리스폰
			if (s.y >= SCREEN_H - 1)
			{
				s.y = 1;
				s.x = rand() % (SCREEN_W - 2) + 1;
				s.speed = rand() % 3 + 1;
				s.ch = starChars[rand() % 4];
				s.color = starColors[rand() % 3];
			}
		}

		// 그리기 (나중에 글자가 덮어쓰므로 글자 안 가림)
		SetChar(s.x, s.y, s.ch, s.color);
	}
}

void Renderer::DrawAttackEffect(int16_t centerX, int16_t centerY, int frame, int16_t myX, int16_t myY)
{
	auto worldToTile = [&](int16_t wx, int16_t wy) -> std::pair<int, int> {
		return { wx - myX + CENTER_TILE_X, wy - myY + CENTER_TILE_Y };
		};

	auto plot = [&](int16_t wx, int16_t wy, wchar_t ch, WORD color) {
		if (wx == centerX && wy == centerY) return;

		auto [tx, ty] = worldToTile(wx, wy);
		if (tx < 0 || tx >= VIEWPORT_TILES) return;
		if (ty < 0 || ty >= VIEWPORT_TILES) return;
		DrawTile(tx, ty, ch, color);
		};

	WORD bright = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
	WORD dim = FOREGROUND_RED | FOREGROUND_INTENSITY;

	switch (frame)
	{
	case 0:
		plot(centerX, centerY - 1, L'.', bright);
		plot(centerX - 1, centerY, L'.', bright);
		plot(centerX + 1, centerY, L'.', bright);
		plot(centerX, centerY + 1, L'.', bright);
		break;

	case 1:
		plot(centerX, centerY - 1, L'*', bright);
		plot(centerX - 1, centerY, L'*', bright);
		plot(centerX + 1, centerY, L'*', bright);
		plot(centerX, centerY + 1, L'*', bright);
		break;

	case 2:
		plot(centerX, centerY - 1, L'|', bright);
		plot(centerX - 1, centerY, L'-', bright);
		plot(centerX + 1, centerY, L'-', bright);
		plot(centerX, centerY + 1, L'|', bright);

		plot(centerX, centerY - 2, L'^', bright);
		plot(centerX - 2, centerY, L'<', bright);
		plot(centerX + 2, centerY, L'>', bright);
		plot(centerX, centerY + 2, L'v', bright);
		break;

	case 3:
		plot(centerX, centerY - 1, L'.', dim);
		plot(centerX - 1, centerY, L'.', dim);
		plot(centerX + 1, centerY, L'.', dim);
		plot(centerX, centerY + 1, L'.', dim);

		plot(centerX, centerY - 2, L'*', dim);
		plot(centerX - 2, centerY, L'*', dim);
		plot(centerX + 2, centerY, L'*', dim);
		plot(centerX, centerY + 2, L'*', dim);
		break;

	case 4:
		plot(centerX, centerY - 2, L'.', dim);
		plot(centerX - 2, centerY, L'.', dim);
		plot(centerX + 2, centerY, L'.', dim);
		plot(centerX, centerY + 2, L'.', dim);
		break;
	}
}

void Renderer::RenderMainMenu(const std::wstring& nameInput, const std::wstring& errorMsg)
{
	Clear();
	++m_frameCount;
	if (!m_starsInited)
	{
		InitStars();
	}

	// 별 비 (먼저 그려서 글자/테두리에 가려지도록)
	UpdateAndDrawStars();

	// 전체 테두리
	DrawBorder(0, 0, SCREEN_W, SCREEN_H);

	// 타이틀 아스키 아트
	auto drawWord = [&](const int* letterIndices, int numLetters, int baseY)
	{
		int totalWidth = 0;
		for (int li = 0; li < numLetters; ++li)
		{
			totalWidth += FONT[letterIndices[li]].width;
			if (li < numLetters - 1) totalWidth += LETTER_GAP;
		}

		int curX = (SCREEN_W - totalWidth) / 2;

		for (int li = 0; li < numLetters; ++li)
		{
			const LetterDef& letter = FONT[letterIndices[li]];

			int yOffset = (int)round(sin(li * 0.8 + m_frameCount * 0.1) * 1.5);
			int colorIdx = ((li + m_frameCount / 4) % NUM_COLORS + NUM_COLORS) % NUM_COLORS;
			WORD color = RAINBOW_COLORS[colorIdx];

			for (int row = 0; row < FONT_ROWS; ++row)
			{
				for (int col = 0; col < letter.width; ++col)
				{
					wchar_t ch = letter.rows[row][col];
					if (ch == L' ') continue;
					SetChar(curX + col, baseY + row + yOffset, ch, color);
				}
			}

			curX += letter.width + LETTER_GAP;
		}
	};

	drawWord(WORD1, WORD1_LEN, 4);
	drawWord(WORD2, WORD2_LEN, 12);

	// 입력 프롬프트
	const wchar_t* prompt = L"Enter your name: ";
	int promptX = 40;
	int promptY = 20;
	DrawString(promptX, promptY, prompt, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

	// 입력 박스: [ ______________ ]
	int boxX = promptX + (int)wcslen(prompt);
	DrawString(boxX, promptY, L"[                  ]", FOREGROUND_INTENSITY);

	// 입력된 이름 표시 (박스 안)
	for (size_t i = 0; i < nameInput.size() && i < 16; ++i)
	{
		SetChar(boxX + 2 + (int)i, promptY, nameInput[i], FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	}

	// 커서 깜빡임
	if (m_frameCount % 30 < 20)
	{
		int cursorX = boxX + 2 + (int)min(nameInput.size(), size_t(16));
		SetChar(cursorX, promptY, L'_', FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	}

	// 안내 텍스트
	DrawString(48, 23, L"[ ENTER ] to login", FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	DrawString(49, 25, L"[ ESC ] to quit", FOREGROUND_RED | FOREGROUND_INTENSITY);
	if (!errorMsg.empty())
	{
		WORD red = FOREGROUND_RED | FOREGROUND_INTENSITY;
		int errorX = (SCREEN_W - static_cast<int>(errorMsg.size())) / 2;
		DrawString(errorX, 27, errorMsg.c_str(), red);
	}

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

void Renderer::PushLog(const std::wstring& line, WORD color)
{
	std::lock_guard lock(m_logMutex);
	m_logLines.push_back({ line, color });
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

	// 시야 밖 영역은 포그로 채움
	WORD fogColor = FOREGROUND_INTENSITY;
	for (int y = VIEWPORT_BORDER_Y + 1; y < VIEWPORT_BORDER_Y + VIEWPORT_BORDER_H - 1; ++y)
	{
		for (int x = VIEWPORT_BORDER_X + 1; x < VIEWPORT_BORDER_X + VIEWPORT_BORDER_W - 1; ++x)
		{
			SetChar(x, y, L'░', fogColor);
		}
	}

	const Map& map = GameState::GetInstance().GetMap();
	WORD treeColor = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
	for (int ty = 0; ty < VIEWPORT_TILES; ++ty)
	{
		for (int tx = 0; tx < VIEWPORT_TILES; ++tx)
		{
			int worldX = tx - CENTER_TILE_X + me.x;
			int worldY = ty - CENTER_TILE_Y + me.y;

			bool outOfBounds = (worldX < 0 || worldX >= MAP_WIDTH ||
				worldY < 0 || worldY >= MAP_HEIGHT);

			if (outOfBounds)
			{
				DrawTile(tx, ty, L' ', 0);     // 맵 밖 — 빨간 블록
			}
			else if (map.IsWalkable(worldX, worldY))
			{
				DrawTile(tx, ty, L'.', emptyColor);   // 통과 가능
			}
			else
			{
				DrawTile(tx, ty, L'♣', treeColor);    // 장애물 — 나무
			}
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
			ch = L'☻';
			color = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
		}
		else
		{
			MonsterVisual mv = GetMonsterVisual(obj.name);
			ch = mv.shape;
			color = mv.baseColor;
		}

		DrawTile(tileX, tileY, ch, color);
	}

	// 내 플레이어 (항상 중앙)
	WORD myColor = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
	DrawTile(CENTER_TILE_X, CENTER_TILE_Y, L'☻', myColor);

	// 공격 이펙트
	auto effects = GameState::GetInstance().GetActiveEffects();
	auto now = std::chrono::steady_clock::now();

	constexpr int FRAME_MS = 80; // 프레임 간격
	constexpr int TOTAL_FRAMES = 5;  // 5단계
	constexpr int LIFETIME_MS = FRAME_MS * TOTAL_FRAMES;

	for (const auto& e : effects)
	{
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - e.startTime).count();
		if (elapsed >= LIFETIME_MS)
		{
			continue;
		}

		int frame = static_cast<int>(elapsed / FRAME_MS);
		DrawAttackEffect(e.x, e.y, frame, me.x, me.y);
	}
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

	// Line 0: Name
	wchar_t nameBuf[32] = { 0 };
	for (size_t i = 0; i < me.name.size() && i < 31; ++i)
	{
		nameBuf[i] = static_cast<wchar_t>(me.name[i]);
	}
	DrawString(HUD_X + 2, HUD_Y + 1, L"Name:", titleColor);
	DrawString(HUD_X + 10, HUD_Y + 1, nameBuf, valueColor);

	// Line 1: Lv
	swprintf_s(buf, L"Lv %d", me.level);
	DrawString(HUD_X + 2, HUD_Y + 3, L"Level:", titleColor);
	DrawString(HUD_X + 10, HUD_Y + 3, buf, valueColor);

	// Line 2: HP
	swprintf_s(buf, L"%d / %d", me.hp, me.maxHp);
	DrawString(HUD_X + 2, HUD_Y + 5, L"HP:", titleColor);
	DrawString(HUD_X + 10, HUD_Y + 5, buf, FOREGROUND_RED | FOREGROUND_INTENSITY);

	// Line 3: Exp
	swprintf_s(buf, L"%d / %d", me.exp, 100 * (1 << (me.level - 1)));
	DrawString(HUD_X + 2, HUD_Y + 7, L"Exp:", titleColor);
	DrawString(HUD_X + 10, HUD_Y + 7, buf, valueColor);

	// Line 4: Pos
	swprintf_s(buf, L"(%d, %d)", me.x, me.y);
	DrawString(HUD_X + 2, HUD_Y + 9, L"Pos:", titleColor);
	DrawString(HUD_X + 10, HUD_Y + 9, buf, valueColor);
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
	for (const auto& entry : m_logLines)
	{
		if (i >= maxContentH)
		{
			break;
		}

		int maxLen = LOG_W - 2;
		std::wstring truncated = entry.text.substr(0, maxLen);

		DrawString(LOG_X + 1, startRow + i, truncated.c_str(), entry.color);
		++i;
	}
}
