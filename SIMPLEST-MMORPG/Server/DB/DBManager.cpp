#include "DBManager.h"
#include "../Logger.h"
#include <iostream>

DBManager& DBManager::GetInstance()
{
	static DBManager instance;
	return instance;
}

bool DBManager::Init(HANDLE iocp,
	const std::string& server,
	const std::string& database,
	const std::string& user,
	const std::string& password)
{
	m_iocp = iocp;

	// DB 연결 실패 시에도 서버는 뜬다: DBConnection이 in-memory fallback으로 동작 (영속화 없음)
	if (!m_loginDB.Connect(server, database, user, password))
	{
		LOG_WARN("[DBManager] DB unavailable -> in-memory mode (progress is NOT persisted)");
	}
	else if (!m_saveDB.Connect(server, database, user, password))
	{
		LOG_WARN("[DBManager] save DB connect failed -> in-memory mode (progress is NOT persisted)");
		m_loginDB.Disconnect();
	}

	m_running = true;

	m_loginThread = std::thread([this]() {
		WorkerThread(m_loginDB, m_loginQueue, m_loginMutex, m_loginCv);
		});
	m_saveThread = std::thread([this]() {
		WorkerThread(m_saveDB, m_saveQueue, m_saveMutex, m_saveCv);
		});

	LOG_INFO("[DBManager] initialized (login + save workers)");
	return true;
}

void DBManager::Shutdown()
{
	{
		std::scoped_lock lock(m_loginMutex, m_saveMutex);
		m_running = false;
	}
	m_loginCv.notify_all();
	m_saveCv.notify_all();

	if (m_loginThread.joinable())
	{
		m_loginThread.join();
	}
	if (m_saveThread.joinable())
	{
		m_saveThread.join();
	}

	m_loginDB.Disconnect();
	m_saveDB.Disconnect();
	LOG_INFO("[DBManager] shutdown complete");
}

void DBManager::PostLoginTask(DBTask task)
{
	{
		std::lock_guard lock(m_loginMutex);
		m_loginQueue.push(std::move(task));
	}
	m_loginCv.notify_one();
}

void DBManager::PostSaveTask(DBTask task)
{
	{
		std::lock_guard lock(m_saveMutex);
		m_saveQueue.push(std::move(task));
	}
	m_saveCv.notify_one();
}

void DBManager::InvokeOnIOCP(IOCPCallback callback)
{
	auto* dbov = new DBCompletionOverlapped;
	ZeroMemory(&dbov->base, sizeof(OverlappedEx));
	dbov->base.ioType = IOType::DB_COMPLETE;
	dbov->callback = std::move(callback);

	BOOL ok = PostQueuedCompletionStatus(m_iocp, 0, 0, &dbov->base.overlapped);
	if (!ok)
	{
		LOG_ERROR("[DBManager] PostQCS failed: " << GetLastError());
		delete dbov;
	}
}

void DBManager::WorkerThread(DBConnection& db,
	std::queue<DBTask>& queue,
	std::mutex& mtx,
	std::condition_variable& cv)
{
	while (true)
	{
		DBTask task;
		{
			std::unique_lock lock(mtx);
			cv.wait(lock, [&] { return !queue.empty() || !m_running; });

			if (!m_running && queue.empty())
			{
				return;
			}

			task = std::move(queue.front());
			queue.pop();
		}

		try
		{
			task(db);
		}
		catch (const std::exception& e)
		{
			LOG_ERROR("[DBManager] task threw: " << e.what());
		}
	}
}