#include "DBManager.h"
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

	if (!m_loginDB.Connect(server, database, user, password))
	{
		std::cerr << "[DBManager] login DB connect failed" << std::endl;
		return false;
	}
	if (!m_saveDB.Connect(server, database, user, password))
	{
		std::cerr << "[DBManager] save DB connect failed" << std::endl;
		return false;
	}

	m_running = true;

	m_loginThread = std::thread([this]() {
		WorkerThread(m_loginDB, m_loginQueue, m_loginMutex, m_loginCv);
		});
	m_saveThread = std::thread([this]() {
		WorkerThread(m_saveDB, m_saveQueue, m_saveMutex, m_saveCv);
		});

	std::cout << "[DBManager] initialized (login + save workers)" << std::endl;
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
	std::cout << "[DBManager] shutdown complete" << std::endl;
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
		std::cerr << "[DBManager] PostQCS failed: " << GetLastError() << std::endl;
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
			std::cerr << "[DBManager] task threw: " << e.what() << std::endl;
		}
	}
}