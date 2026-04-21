#include <iostream>
#include <string>
#include "NetworkClient.h"
#include "Protocol.h"
#include "Constants.h"
#include "Logger.h"
#include "Renderer.h"

int main()
{
	/*NetworkClient client;

	if (!client.Connect("127.0.0.1", SERVER_PORT))
	{
		LOG("Connect failed");
		return -1;
	}*/

	Renderer& r = Renderer::GetInstance();
	if (!r.Init())
	{
		LOG("Renderer init failed");
		return -1;
	}

	// 가짜 이름 입력 상태로 메인 메뉴 렌더
	std::wstring nameInput = L"hero";
	r.RenderMainMenu(nameInput);
	Sleep(5000);

	//client.Disconnect();
}