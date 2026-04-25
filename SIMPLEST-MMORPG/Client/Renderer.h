#pragma once
#include <Windows.h>
#include <string>
#include <deque>
#include <mutex>

class Renderer
{
public:
	static Renderer& GetInstance();

	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;

	bool Init();

	// 화면별 렌더링 (상태 머신이 호출)
	void RenderMainMenu(const std::wstring& nameInput);
	void RenderGame();

	// 로그/채팅 창에 한 줄 추가 (Thread-Safe)
	void PushLog(const std::wstring& line, WORD color = (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY));

private:
	Renderer() = default;
	~Renderer() = default;

	// 내부 헬퍼
	void Clear();
	void Flush();
	void SetChar(int x, int y, wchar_t ch, WORD attr);
	void DrawString(int x, int y, const wchar_t* str, WORD attr);
	void DrawBorder(int x, int y, int w, int h);
	void DrawTile(int tileX, int tileY, wchar_t ch, WORD attr);

	// 게임 화면 서브 렌더
	void DrawViewport();
	void DrawHUD();
	void DrawLogBox();

	// 별 비 효과
	void InitStars();
	void UpdateAndDrawStars();

	// 공격 이펙트
	void DrawAttackEffect(int16_t centerX, int16_t centerY, int frame, int16_t myX, int16_t myY);

private:
	HANDLE m_hConsole{ INVALID_HANDLE_VALUE };

	static constexpr int SCREEN_W = 120;
	static constexpr int SCREEN_H = 30;
	CHAR_INFO m_buffer[SCREEN_W * SCREEN_H]{};

	struct LogLine
	{
		std::wstring text;
		WORD color;
	};
	static constexpr size_t MAX_LOG_LINES = 15;
	std::deque<LogLine> m_logLines;
	mutable std::mutex m_logMutex;

	// 별 비 효과
	struct Star 
	{
		int x;
		int y;
		int speed; // 몇 프레임마다 1칸 낙하
		int ticker; // 현재 틱
		wchar_t ch;
		WORD color;
	};
	static constexpr int MAX_STARS = 30;
	Star m_stars[MAX_STARS]{};
	bool m_starsInited{ false };
	int m_frameCount{ 0 };
};

