#include "mysqlconnection.hpp"


MysqlConnection::MysqlConnection()
{
	_conn = mysql_init(nullptr);
	_alivetime = 0;
}

MysqlConnection::~MysqlConnection()
{
	if (_conn != nullptr)
	{
		mysql_close(_conn);
	}
}

bool MysqlConnection::connect(string ip, unsigned short port,
	string username, string password, string dbname)
{
	MYSQL* p = mysql_real_connect(_conn, ip.c_str(), username.c_str(),
		password.c_str(), dbname.c_str(), port, nullptr, 0);
	return p != nullptr;
}
 
bool MysqlConnection::update(string sql)
{
	if (mysql_query(_conn, sql.c_str()))
	{
		return false;
	}
	return true;
}

MYSQL_RES* MysqlConnection::query(string sql)
{
	if (mysql_query(_conn, sql.c_str()))
	{
		return nullptr;
	}
	return mysql_use_result(_conn);
}

void MysqlConnection::refreshAliveTime()
{
	_alivetime = clock();
}

clock_t MysqlConnection::getAliveTime() const
{
	return clock() - _alivetime;
}

MYSQL* MysqlConnection::getConnection() const
{
    return _conn;
}