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
	std::wstring errorMsg;
	std::wstring chatBuffer;
	AppState state = AppState::MAIN_MENU;
	ChatChannel chatChannel = ChatChannel::VIEW;
	
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
				errorMsg.clear();
				state = AppState::CONNECTING;
				break;
			}

			renderer.RenderMainMenu(nameInput, errorMsg);
			break;
		}
		case AppState::CONNECTING:
		{
			if (!client.Connect(SERVER_IP, SERVER_PORT))
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
			uint8_t reason = 0;
			if (client.ConsumeLoginFail(reason))
			{
				client.Disconnect();
				switch (static_cast<LoginFailReason>(reason))
				{
				case LoginFailReason::DUPLICATE_LOGIN:
					errorMsg = L"This character is already connected.";
					break;
				case LoginFailReason::DB_ERROR:
					errorMsg = L"This is a server database error.";
					break;
				case LoginFailReason::SPAWN_FULL:
					errorMsg = L"The spawn location is full.";
					break;
				default:
					errorMsg = L"Login failed.";
					break;
				}
				state = AppState::MAIN_MENU;
				break;
			}

			auto& input = InputHandler::GetInstance();
			if (input.IsChatMode())
			{
				auto chatInput = input.ProcessChatInput(chatBuffer);

				if (chatInput.cancel)
				{
					chatBuffer.clear();
					input.ExitChatMode();
				}
				else if (chatInput.channelToggled)
				{
					chatChannel = (chatChannel == ChatChannel::VIEW)
						? ChatChannel::GLOBAL : ChatChannel::VIEW;
				}
				else if (chatInput.submit)
				{
					if (!chatBuffer.empty())
					{
						CS_Chat pkt;
						pkt.header.size = sizeof(pkt);
						pkt.header.type = static_cast<uint16_t>(PacketType::CS_CHAT);
						pkt.channel = static_cast<uint8_t>(chatChannel);

						std::string msg;
						msg.reserve(chatBuffer.size());
						for (wchar_t wc : chatBuffer)
						{
							msg.push_back(static_cast<char>(wc));
						}
						strncpy_s(pkt.message, sizeof(pkt.message), msg.c_str(), _TRUNCATE);

						client.SendPacket(&pkt, sizeof(pkt));
					}

					chatBuffer.clear();
					input.ExitChatMode();
				}
			}
			else
			{
				auto gameInput = input.ProcessGameInput();
				if (gameInput.escPressed)
				{
					state = AppState::DISCONNECTED;
					break;
				}
				if (gameInput.chatModeEntered)
				{
					input.EnterChatMode();
				}
				
				if (gameInput.hasMove)
				{
					GameState& gameState = GameState::GetInstance();
					MyPlayer me = gameState.GetMyPlayer();
					int16_t newX = me.x + DX[static_cast<int>(gameInput.moveDir)];
					int16_t newY = me.y + DY[static_cast<int>(gameInput.moveDir)];

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
						mp.direction = static_cast<uint8_t>(gameInput.moveDir);
						mp.move_time = 0;  // 일반 클라는 latency 측정 안 함
						client.SendPacket(&mp, sizeof(mp));

						// 로컬 예측
						gameState.MoveMyPlayer(newX, newY);
					}
				}

				if (gameInput.attackPressed)
				{
					CS_Attack atk;
					atk.header.size = sizeof(atk);
					atk.header.type = static_cast<uint16_t>(PacketType::CS_ATTACK);
					client.SendPacket(&atk, sizeof(atk));
				}
			}

			renderer.RenderGame(input.IsChatMode(), chatChannel, chatBuffer);
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