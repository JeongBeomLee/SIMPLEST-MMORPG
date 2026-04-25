#include "NetworkClient.h"
#include "Renderer.h"
#include "InputHandler.h"
#include "GameState.h"
#include "Protocol.h"
#include "Constants.h"
#include "Logger.h"
#include <chrono>
#include <Windows.h>

enum class AppState 
{
	MAIN_MENU,
	CONNECTING,
	IN_GAME,
	DISCONNECTED
};

int main()
{
	Renderer& renderer = Renderer::GetInstance();
	if (!renderer.Init())
	{
		return -1;
	}

	if (!GameState::GetInstance().Init()) {
		LOG("GameState init failed - map file not found");
		return -1;
	}

	NetworkClient client;
	std::wstring nameInput;
	AppState state = AppState::MAIN_MENU;
	
	while (state != AppState::DISCONNECTED)
	{
		auto frameStart = std::chrono::steady_clock::now();

		switch (state)
		{
		case AppState::MAIN_MENU:
		{
			auto input = InputHandler::GetInstance().ProcessMainMenuInput(nameInput);
			if (input.escPressed)
			{
				state = AppState::DISCONNECTED;
				break;
			}

			if (input.enterPressed && !nameInput.empty())
			{
				state = AppState::CONNECTING;
				break;
			}

			renderer.RenderMainMenu(nameInput);
			break;
		}
		case AppState::CONNECTING:
		{
			if (!client.Connect("127.0.0.1", SERVER_PORT))
			{
				state = AppState::MAIN_MENU;
				break;
			}

			CS_Login pkt;
			pkt.header.size = sizeof(pkt);
			pkt.header.type = static_cast<uint16_t>(PacketType::CS_LOGIN);

			std::string nameUtf8;
			nameUtf8.reserve(nameInput.size());
			for (wchar_t wc : nameInput)
			{
				nameUtf8.push_back(static_cast<char>(wc));
			}

			strncpy_s(pkt.name, sizeof(pkt.name), nameUtf8.c_str(), _TRUNCATE);
			client.SendPacket(&pkt, sizeof(pkt));

			GameState::GetInstance().SetMyName(nameUtf8);

			state = AppState::IN_GAME;
			break;
		}
		case AppState::IN_GAME:
		{
			auto input = InputHandler::GetInstance().ProcessGameInput();
			if (input.escPressed) 
			{
				state = AppState::DISCONNECTED;
				break;
			}

			if (input.hasMove)
			{
				GameState& gameState = GameState::GetInstance();
				MyPlayer me = gameState.GetMyPlayer();
				int16_t newX = me.x + DX[static_cast<int>(input.moveDir)];
				int16_t newY = me.y + DY[static_cast<int>(input.moveDir)];

				bool canMove = gameState.GetMap().IsWalkable(newX, newY);
				if (canMove)
				{
					auto objects = gameState.GetAllObjects();
					for (const auto& obj : objects)
					{
						if (obj.x == newX && obj.y == newY)
						{
							canMove = false;
							break;
						}
					}
				}

				if (canMove)
				{
					// 서버 전송
					CS_Move mp;
					mp.header.size = sizeof(mp);
					mp.header.type = static_cast<uint16_t>(PacketType::CS_MOVE);
					mp.direction = static_cast<uint8_t>(input.moveDir);
					client.SendPacket(&mp, sizeof(mp));

					// 로컬 예측
					gameState.MoveMyPlayer(newX, newY);
				}
			}
			
			if (input.attackPressed)
			{
				CS_Attack atk;
				atk.header.size = sizeof(atk);
				atk.header.type = static_cast<uint16_t>(PacketType::CS_ATTACK);
				client.SendPacket(&atk, sizeof(atk));
			}

			renderer.RenderGame();
			break;
		}
		}

		auto frameEnd = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart).count();
		int frameMs = 33;
		if (elapsed < frameMs)
		{
			Sleep(frameMs - (int)elapsed);
		}
	}
	
	client.Disconnect();
}