#pragma once
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <string>

struct PlayerRow
{
	int64_t  id;
	std::string name;
	int16_t  level;
	int32_t  exp;
	int32_t  hp;
	int32_t  maxHp;
	int16_t  x;
	int16_t  y;
};

class DBConnection
{
public:
	DBConnection();
	~DBConnection();

	DBConnection(const DBConnection&) = delete;
	DBConnection& operator=(const DBConnection&) = delete;

	// 연결 / 해제
	bool Connect(const std::string& server, 
		         const std::string& database, 
		         const std::string& user, 
		         const std::string& password);
	void Disconnect();

	bool IsConnected() const { return m_conn != SQL_NULL_HDBC; }

	// SP 호출
	bool LoadPlayerByName(const std::string& name, PlayerRow& outRow, bool& outFound);
	bool CreatePlayer(const std::string& name, int64_t& outId);
	bool SavePlayer(const PlayerRow& row);
	bool UpdateLastLogin(int64_t id);

private:
	void PrintDiagnostic(SQLSMALLINT handleType, SQLHANDLE handle, const char* context);

private:
	SQLHENV  m_env = SQL_NULL_HENV;
	SQLHDBC  m_conn = SQL_NULL_HDBC;
};