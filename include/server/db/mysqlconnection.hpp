#pragma once
#include<mysql/mysql.h>
#include<string>
#include<ctime>
using namespace std;

class MysqlConnection
{
public:
	//初始化数据库连接
	MysqlConnection();
	//释放数据库连接资源
	~MysqlConnection();
	//连接数据库
	bool connect(string ip,
		unsigned short port,
		string username,
		string password,
		string dbname);
	//更新操作
	bool update(string sql);
	//查询操作
	MYSQL_RES* query(string sql);
	//刷新一下连接的起始的空闲时间点
	void refreshAliveTime();
	//返回存活时间
	clock_t getAliveTime() const;
    //返回连接
    MYSQL* getConnection() const;
private:
	//表示和MySQL Server的一条连接
	MYSQL* _conn;
	clock_t _alivetime;
};