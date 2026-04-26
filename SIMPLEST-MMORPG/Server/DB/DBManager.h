#pragma once
#include "DBConnection.h"
#include "../Network/NetworkTypes.h"
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

class DBManager
{
public:
	static DBManager& GetInstance();

	DBManager(const DBManager&) = delete;
	DBManager& operator=(const DBManager&) = delete;

	using DBTask = std::function<void(DBConnection&)>;
	using IOCPCallback = std::function<void()>;

	bool Init(HANDLE iocp,
		const std::string& server,
		const std::string& database,
		const std::string& user,
		const std::string& password);
	void Shutdown();

	// IOCP 워커가 호출
	void PostLoginTask(DBTask task);
	void PostSaveTask(DBTask task);

	// DB 워커가 결과를 IOCP 워커로 던질 때 호출
	void InvokeOnIOCP(IOCPCallback callback);

private:
	DBManager() = default;
	~DBManager() = default;

	void WorkerThread(DBConnection& db,
		std::queue<DBTask>& queue,
		std::mutex& mtx,
		std::condition_variable& cv);

private:
	HANDLE m_iocp = nullptr;

	DBConnection m_loginDB;
	DBConnection m_saveDB;

	std::queue<DBTask> m_loginQueue;
	std::queue<DBTask> m_saveQueue;
	std::mutex m_loginMutex;
	std::mutex m_saveMutex;
	std::condition_variable m_loginCv;
	std::condition_variable m_saveCv;

	std::thread m_loginThread;
	std::thread m_saveThread;

	std::atomic<bool> m_running{ false };
};