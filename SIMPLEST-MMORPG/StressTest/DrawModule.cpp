// OpenGL 시각화 — 봇 위치를 점으로 표시
// 베이스: NeHe Bitmap Font Tutorial (legacy GL fixed pipeline)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <gl/gl.h>
#include <gl/glu.h>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

#include "NetworkModule.h"

static HDC       hDC       = NULL;
static HGLRC     hRC       = NULL;
static HWND      hWnd      = NULL;
static HINSTANCE hInstance = NULL;

static GLuint    g_fontBase;
static bool      g_keys[256];
static bool      g_active = true;

constexpr int WIN_W   = 800;
constexpr int WIN_H   = 800;
constexpr float MAP_W = 2000.0f;
constexpr float MAP_H = 2000.0f;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// ============================================================
// 폰트
// ============================================================
static void BuildFont()
{
	HFONT font;
	g_fontBase = glGenLists(96);
	font = CreateFontA(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
	                   ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
	                   ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH,
	                   "Courier New");
	HFONT old = (HFONT)SelectObject(hDC, font);
	wglUseFontBitmapsA(hDC, 32, 96, g_fontBase);
	SelectObject(hDC, old);
	DeleteObject(font);
}

static void KillFont()
{
	glDeleteLists(g_fontBase, 96);
}

static void glPrint(const char* fmt, ...)
{
	char text[256];
	va_list ap;
	if (!fmt) return;
	va_start(ap, fmt);
	vsprintf_s(text, sizeof(text), fmt, ap);
	va_end(ap);

	glPushAttrib(GL_LIST_BIT);
	glListBase(g_fontBase - 32);
	glCallLists((GLsizei)strlen(text), GL_UNSIGNED_BYTE, text);
	glPopAttrib();
}

// ============================================================
// GL 셋업
// ============================================================
static void ResizeScene(GLsizei w, GLsizei h)
{
	if (h == 0) h = 1;
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0f, (GLfloat)w / (GLfloat)h, 0.1f, 100.0f);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

static int InitGL()
{
	glShadeModel(GL_SMOOTH);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	BuildFont();
	return TRUE;
}

static int DrawGLScene()
{
	int    size   = 0;
	float* points = nullptr;
	GetPointCloud(&size, &points);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	glTranslatef(0.0f, 0.0f, -2.5f);

	// 통계 텍스트 (좌측 상단 — perspective 절단면 안쪽)
	glColor3f(1.0f, 1.0f, 0.0f);
	glRasterPos2f(-0.95f, 0.92f);
	glPrint("STRESS TEST  active=%d", (int)active_clients);
	glRasterPos2f(-0.95f, 0.85f);
	glPrint("Delay: %d ms", global_delay);

	// 봇 위치 점 (맵 0~2000 → [-1, 1])
	glColor3f(0.4f, 1.0f, 0.4f);
	glPointSize(2.0f);
	glBegin(GL_POINTS);
	for (int i = 0; i < size; ++i)
	{
		float x = points[i * 2 + 0] / (MAP_W * 0.5f) - 1.0f;
		float y = 1.0f - points[i * 2 + 1] / (MAP_H * 0.5f);
		glVertex3f(x, y, 0.0f);
	}
	glEnd();

	// 맵 경계 박스
	glColor3f(0.3f, 0.3f, 0.5f);
	glBegin(GL_LINE_LOOP);
	glVertex3f(-1.0f, -1.0f, 0.0f);
	glVertex3f( 1.0f, -1.0f, 0.0f);
	glVertex3f( 1.0f,  1.0f, 0.0f);
	glVertex3f(-1.0f,  1.0f, 0.0f);
	glEnd();

	return TRUE;
}

// ============================================================
// 윈도우 / GL 컨텍스트
// ============================================================
static void KillGLWindow()
{
	if (hRC)
	{
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(hRC);
		hRC = NULL;
	}
	if (hDC && hWnd) ReleaseDC(hWnd, hDC);
	hDC = NULL;
	if (hWnd) DestroyWindow(hWnd);
	hWnd = NULL;
	UnregisterClassW(L"OpenGLStress", hInstance);
	KillFont();
}

static BOOL CreateGLWindow(const wchar_t* title, int width, int height, BYTE bits)
{
	GLuint   pixelFormat;
	WNDCLASSW wc;
	DWORD    dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
	DWORD    dwStyle   = WS_OVERLAPPEDWINDOW;

	RECT rect = { 0, 0, width, height };
	AdjustWindowRectEx(&rect, dwStyle, FALSE, dwExStyle);

	hInstance = GetModuleHandle(NULL);
	wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc   = (WNDPROC)WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = L"OpenGLStress";

	if (!RegisterClassW(&wc)) return FALSE;

	hWnd = CreateWindowExW(dwExStyle, L"OpenGLStress", title,
	                       dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
	                       100, 100,
	                       rect.right - rect.left, rect.bottom - rect.top,
	                       NULL, NULL, hInstance, NULL);
	if (!hWnd) { KillGLWindow(); return FALSE; }

	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR), 1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA, bits,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		16, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
	};

	hDC = GetDC(hWnd);
	if (!hDC) { KillGLWindow(); return FALSE; }
	pixelFormat = ChoosePixelFormat(hDC, &pfd);
	if (!pixelFormat) { KillGLWindow(); return FALSE; }
	if (!SetPixelFormat(hDC, pixelFormat, &pfd)) { KillGLWindow(); return FALSE; }
	hRC = wglCreateContext(hDC);
	if (!hRC) { KillGLWindow(); return FALSE; }
	if (!wglMakeCurrent(hDC, hRC)) { KillGLWindow(); return FALSE; }

	ShowWindow(hWnd, SW_SHOW);
	SetForegroundWindow(hWnd);
	SetFocus(hWnd);
	ResizeScene(width, height);
	if (!InitGL()) { KillGLWindow(); return FALSE; }

	return TRUE;
}

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
	switch (msg)
	{
	case WM_ACTIVATE:
		g_active = (HIWORD(w) == 0);
		return 0;
	case WM_SYSCOMMAND:
		if (w == SC_SCREENSAVE || w == SC_MONITORPOWER) return 0;
		break;
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
		g_keys[w] = TRUE;
		return 0;
	case WM_KEYUP:
		g_keys[w] = FALSE;
		return 0;
	case WM_SIZE:
		ResizeScene(LOWORD(l), HIWORD(l));
		return 0;
	}
	return DefWindowProc(h, msg, w, l);
}

// ============================================================
// 엔트리
// ============================================================
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	if (!CreateGLWindow(L"MMORPG Stress Test", WIN_W, WIN_H, 32))
	{
		return 0;
	}

	InitializeNetwork();

	MSG  msg;
	BOOL done = FALSE;
	while (!done)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT) done = TRUE;
			else { TranslateMessage(&msg); DispatchMessage(&msg); }
		}
		else
		{
			if (g_active && !g_keys[VK_ESCAPE])
			{
				DrawGLScene();
				SwapBuffers(hDC);
			}
			else if (g_keys[VK_ESCAPE])
			{
				done = TRUE;
			}
			Sleep(16);  // ~60fps
		}
	}

	ShutdownNetwork();
	KillGLWindow();
	return (int)msg.wParam;
}

int main()
{
	return WinMain(NULL, NULL, NULL, 0);
}
