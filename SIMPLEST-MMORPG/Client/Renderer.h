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
	void PushLog(const std::wstring& line);

private:
	Renderer() = default;
	~Renderer() = default;

	// 내부 헬퍼
	void Clear();
	void Flush();
	void SetChar(int x, int y, wchar_t ch, WORD attr);
	void DrawString(int x, int y, const wchar_t* str, WORD attr);
	void DrawBorder(int x, int y, int w, int h);

	// 게임 화면 서브 렌더
	void DrawViewport();
	void DrawHUD();
	void DrawLogBox();

private:
	HANDLE m_hConsole{ INVALID_HANDLE_VALUE };

	static constexpr int SCREEN_W = 120;
	static constexpr int SCREEN_H = 30;
	CHAR_INFO m_buffer[SCREEN_W * SCREEN_H]{};

	static constexpr size_t MAX_LOG_LINES = 18;
	std::deque<std::wstring> m_logLines;
	mutable std::mutex m_logMutex;
};

