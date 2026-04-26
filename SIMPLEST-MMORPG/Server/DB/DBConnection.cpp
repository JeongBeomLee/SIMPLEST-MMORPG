#include "DBConnection.h"
#include "../Logger.h"
#include <iostream>
#include <sstream>

DBConnection::DBConnection()
{
	// 환경 핸들 할당
	SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_env);
	if (!SQL_SUCCEEDED(ret))
	{
		LOG_ERROR("[DB] SQLAllocHandle(ENV) failed");
		m_env = SQL_NULL_HENV;
		return;
	}

	// ODBC 버전 선언
	ret = SQLSetEnvAttr(m_env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
	if (!SQL_SUCCEEDED(ret))
	{
		LOG_ERROR("[DB] SQLSetEnvAttr failed");
		SQLFreeHandle(SQL_HANDLE_ENV, m_env);
		m_env = SQL_NULL_HENV;
		return;
	}
}

DBConnection::~DBConnection()
{
	Disconnect();

	if (m_env != SQL_NULL_HENV)
	{
		SQLFreeHandle(SQL_HANDLE_ENV, m_env);
		m_env = SQL_NULL_HENV;
	}
}

bool DBConnection::Connect(const std::string& server, 
	                       const std::string& database, 
	                       const std::string& user, 
	                       const std::string& password)
{
	if (m_env == SQL_NULL_HENV)
	{
		LOG_ERROR("[DB] env not initialized");
		return false;
	}

	// 연결 핸들 할당
	SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_DBC, m_env, &m_conn);
	if (!SQL_SUCCEEDED(ret))
	{
		PrintDiagnostic(SQL_HANDLE_ENV, m_env, "AllocHandle(DBC)");
		m_conn = SQL_NULL_HDBC;
		return false;
	}

	// Connection string 조립 (Driver 18 + 서버 인증서 신뢰)
	std::ostringstream oss;
	oss << "DRIVER={ODBC Driver 18 for SQL Server};"
		<< "SERVER=" << server << ";"
		<< "DATABASE=" << database << ";"
		<< "UID=" << user << ";"
		<< "PWD=" << password << ";"
		<< "TrustServerCertificate=yes;";
	std::string connStr = oss.str();

	// 연결 시도
	SQLCHAR outConnStr[1024];
	SQLSMALLINT outConnStrLen = 0;
	ret = SQLDriverConnectA(m_conn,
		nullptr,
		(SQLCHAR*)connStr.c_str(),
		(SQLSMALLINT)connStr.size(),
		outConnStr,
		sizeof(outConnStr),
		&outConnStrLen,
		SQL_DRIVER_NOPROMPT);

	if (!SQL_SUCCEEDED(ret))
	{
		PrintDiagnostic(SQL_HANDLE_DBC, m_conn, "SQLDriverConnect");
		SQLFreeHandle(SQL_HANDLE_DBC, m_conn);
		m_conn = SQL_NULL_HDBC;
		return false;
	}

	LOG_INFO("[DB] Connected to " << server << "/" << database);
	return true;
}

void DBConnection::Disconnect()
{
	if (m_conn != SQL_NULL_HDBC)
	{
		SQLDisconnect(m_conn);
		SQLFreeHandle(SQL_HANDLE_DBC, m_conn);
		m_conn = SQL_NULL_HDBC;
		LOG_INFO("[DB] Disconnected");
	}
}

bool DBConnection::LoadPlayerByName(const std::string& name, PlayerRow& outRow, bool& outFound)
{
	outFound = false;
	if (m_conn == SQL_NULL_HDBC)
	{
		return false;
	}

	// statement 핸들 할당
	SQLHSTMT stmt = SQL_NULL_HSTMT;
	SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, m_conn, &stmt);
	if (!SQL_SUCCEEDED(ret))
	{
		PrintDiagnostic(SQL_HANDLE_DBC, m_conn, "AllocStmt(LoadPlayer)");
		return false;
	}

	// SP 준비
	SQLCHAR query[] = "{CALL dbo.sp_LoadPlayerByName(?)}";
	ret = SQLPrepareA(stmt, query, SQL_NTS);
	if (!SQL_SUCCEEDED(ret))
	{
		PrintDiagnostic(SQL_HANDLE_STMT, stmt, "Prepare(LoadPlayer)");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	// 입력 파라미터 바인딩 (@name → SQL_WVARCHAR로 ODBC가 ANSI→UTF16 변환)
	SQLLEN nameLen = SQL_NTS;
	ret = SQLBindParameter(stmt, 1,
		SQL_PARAM_INPUT,
		SQL_C_CHAR,           // 입력 버퍼 타입 (C++ 측)
		SQL_WVARCHAR,         // SP 파라미터 타입 (SQL 측)
		32,                   // 컬럼 크기 (NVARCHAR(32))
		0,
		(SQLPOINTER)name.c_str(),
		(SQLLEN)name.size(),
		&nameLen);
	if (!SQL_SUCCEEDED(ret))
	{
		PrintDiagnostic(SQL_HANDLE_STMT, stmt, "BindParam(LoadPlayer)");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	// 실행
	ret = SQLExecute(stmt);
	if (!SQL_SUCCEEDED(ret))
	{
		PrintDiagnostic(SQL_HANDLE_STMT, stmt, "Execute(LoadPlayer)");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	// 결과 컬럼 바인딩 (SP의 SELECT 순서: id, name, level, exp, hp, max_hp, x, y)
	SQLBIGINT id = 0;
	SQLCHAR nameBuf[33] = { 0 };
	SQLSMALLINT level = 0;
	SQLINTEGER exp = 0;
	SQLINTEGER hp = 0;
	SQLINTEGER maxHp = 0;
	SQLSMALLINT x = 0;
	SQLSMALLINT y = 0;
	SQLLEN ind_id, ind_name, ind_level, ind_exp, ind_hp, ind_maxHp, ind_x, ind_y;

	SQLBindCol(stmt, 1, SQL_C_SBIGINT, &id, sizeof(id), &ind_id);
	SQLBindCol(stmt, 2, SQL_C_CHAR, nameBuf, sizeof(nameBuf), &ind_name);
	SQLBindCol(stmt, 3, SQL_C_SSHORT, &level, sizeof(level), &ind_level);
	SQLBindCol(stmt, 4, SQL_C_SLONG, &exp, sizeof(exp), &ind_exp);
	SQLBindCol(stmt, 5, SQL_C_SLONG, &hp, sizeof(hp), &ind_hp);
	SQLBindCol(stmt, 6, SQL_C_SLONG, &maxHp, sizeof(maxHp), &ind_maxHp);
	SQLBindCol(stmt, 7, SQL_C_SSHORT, &x, sizeof(x), &ind_x);
	SQLBindCol(stmt, 8, SQL_C_SSHORT, &y, sizeof(y), &ind_y);

	// 한 행 fetch (이름 UNIQUE라 0 또는 1행)
	ret = SQLFetch(stmt);
	if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
	{
		outRow.id = id;
		outRow.name = std::string(reinterpret_cast<char*>(nameBuf));
		outRow.level = level;
		outRow.exp = exp;
		outRow.hp = hp;
		outRow.maxHp = maxHp;
		outRow.x = x;
		outRow.y = y;
		outFound = true;
	}
	else if (ret != SQL_NO_DATA)
	{
		PrintDiagnostic(SQL_HANDLE_STMT, stmt, "Fetch(LoadPlayer)");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	return true;
}

bool DBConnection::CreatePlayer(const std::string& name, int64_t& outId)
{
	outId = 0;
	if (m_conn == SQL_NULL_HDBC)
	{
		return false;
	}

	SQLHSTMT stmt = SQL_NULL_HSTMT;
	SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, m_conn, &stmt);
	if (!SQL_SUCCEEDED(ret))
	{
		PrintDiagnostic(SQL_HANDLE_DBC, m_conn, "AllocStmt(CreatePlayer)");
		return false;
	}

	SQLCHAR query[] = "{CALL dbo.sp_CreatePlayer(?)}";
	ret = SQLPrepareA(stmt, query, SQL_NTS);
	if (!SQL_SUCCEEDED(ret))
	{
		PrintDiagnostic(SQL_HANDLE_STMT, stmt, "Prepare(CreatePlayer)");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	SQLLEN nameLen = SQL_NTS;
	ret = SQLBindParameter(stmt, 1,
		SQL_PARAM_INPUT,
		SQL_C_CHAR, SQL_WVARCHAR,
		32, 0,
		(SQLPOINTER)name.c_str(),
		(SQLLEN)name.size(),
		&nameLen);
	if (!SQL_SUCCEEDED(ret))
	{
		PrintDiagnostic(SQL_HANDLE_STMT, stmt, "BindParam(CreatePlayer)");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	ret = SQLExecute(stmt);
	if (!SQL_SUCCEEDED(ret))
	{
		// 중복 이름 등의 에러도 여기서 진단됨
		PrintDiagnostic(SQL_HANDLE_STMT, stmt, "Execute(CreatePlayer)");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	// SP가 SELECT SCOPE_IDENTITY()로 id를 반환
	SQLBIGINT newId = 0;
	SQLLEN ind_id = 0;
	SQLBindCol(stmt, 1, SQL_C_SBIGINT, &newId, sizeof(newId), &ind_id);

	ret = SQLFetch(stmt);
	if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
	{
		outId = newId;
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return true;
	}

	PrintDiagnostic(SQL_HANDLE_STMT, stmt, "Fetch(CreatePlayer)");
	SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	return false;
}

bool DBConnection::SavePlayer(const PlayerRow& row)
{
	if (m_conn == SQL_NULL_HDBC)
	{
		return false;
	}

	SQLHSTMT stmt = SQL_NULL_HSTMT;
	SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, m_conn, &stmt);
	if (!SQL_SUCCEEDED(ret))
	{
		PrintDiagnostic(SQL_HANDLE_DBC, m_conn, "AllocStmt(SavePlayer)");
		return false;
	}

	SQLCHAR query[] = "{CALL dbo.sp_SavePlayer(?, ?, ?, ?, ?, ?, ?)}";
	ret = SQLPrepareA(stmt, query, SQL_NTS);
	if (!SQL_SUCCEEDED(ret))
	{
		PrintDiagnostic(SQL_HANDLE_STMT, stmt, "Prepare(SavePlayer)");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	// 파라미터 바인딩 + 에러 체크
	auto bindOrFail = [&](SQLUSMALLINT idx, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLPOINTER ptr) -> bool
		{
			SQLRETURN r = SQLBindParameter(stmt, idx, SQL_PARAM_INPUT, cType, sqlType, 0, 0, ptr, 0, nullptr);
			if (!SQL_SUCCEEDED(r))
			{
				PrintDiagnostic(SQL_HANDLE_STMT, stmt, "BindParam(SavePlayer)");
				return false;
			}
			return true;
		};

	if (!bindOrFail(1, SQL_C_SBIGINT, SQL_BIGINT, (SQLPOINTER)&row.id) ||
		!bindOrFail(2, SQL_C_SSHORT, SQL_SMALLINT, (SQLPOINTER)&row.level) ||
		!bindOrFail(3, SQL_C_SLONG, SQL_INTEGER, (SQLPOINTER)&row.exp) ||
		!bindOrFail(4, SQL_C_SLONG, SQL_INTEGER, (SQLPOINTER)&row.hp) ||
		!bindOrFail(5, SQL_C_SLONG, SQL_INTEGER, (SQLPOINTER)&row.maxHp) ||
		!bindOrFail(6, SQL_C_SSHORT, SQL_SMALLINT, (SQLPOINTER)&row.x) ||
		!bindOrFail(7, SQL_C_SSHORT, SQL_SMALLINT, (SQLPOINTER)&row.y))
	{
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	ret = SQLExecute(stmt);
	if (!SQL_SUCCEEDED(ret))
	{
		PrintDiagnostic(SQL_HANDLE_STMT, stmt, "Execute(SavePlayer)");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	return true;
}

bool DBConnection::UpdateLastLogin(int64_t id)
{
	if (m_conn == SQL_NULL_HDBC)
	{
		return false;
	}

	SQLHSTMT stmt = SQL_NULL_HSTMT;
	SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, m_conn, &stmt);
	if (!SQL_SUCCEEDED(ret))
	{
		PrintDiagnostic(SQL_HANDLE_DBC, m_conn, "AllocStmt(UpdateLastLogin)");
		return false;
	}

	SQLCHAR query[] = "{CALL dbo.sp_UpdateLastLogin(?)}";
	ret = SQLPrepareA(stmt, query, SQL_NTS);
	if (!SQL_SUCCEEDED(ret))
	{
		PrintDiagnostic(SQL_HANDLE_STMT, stmt, "Prepare(UpdateLastLogin)");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	SQLBIGINT idVal = id;
	SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT, 0, 0, &idVal, 0, nullptr);

	ret = SQLExecute(stmt);
	if (!SQL_SUCCEEDED(ret))
	{
		PrintDiagnostic(SQL_HANDLE_STMT, stmt, "Execute(UpdateLastLogin)");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	return true;
}

void DBConnection::PrintDiagnostic(SQLSMALLINT handleType, SQLHANDLE handle, const char* context)
{
	SQLCHAR     sqlState[6] = { 0 };
	SQLINTEGER  nativeErr = 0;
	SQLCHAR     msg[SQL_MAX_MESSAGE_LENGTH] = { 0 };
	SQLSMALLINT msgLen = 0;
	SQLSMALLINT recNum = 1;

	while (SQLGetDiagRecA(handleType, handle, recNum,
		sqlState, &nativeErr,
		msg, sizeof(msg), &msgLen) == SQL_SUCCESS)
	{
		LOG_ERROR("[DB][" << context << "] SQLSTATE=" << sqlState
		          << " native=" << nativeErr << " msg=" << msg);
		++recNum;
	}
}