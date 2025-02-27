#pragma once
#include<iostream>
#include<queue>
#include<mutex>
#include<condition_variable>
#include<thread>
#include<string>
#include<atomic>
#include<functional>
#include<memory>
#include"mysqlconnection.hpp"
using namespace std;

class ConnectionPool
{
public:
	//获取连接池对象实例
	static ConnectionPool* GetConnectionPool();
	//给外部提供接口，从连接池中获取一个可用的空闲连接
	shared_ptr<MysqlConnection> getConnection();

private:
	//连接池初始化
	ConnectionPool();
	//运行在独立的线程中，专门负责生产新连接
	void produceConnectionTask();
	//扫描超过maxIdleTime时间的空闲连接，进行对于的连接回收
	void checkConnectionTask();

	string _ip;//ip地址
	unsigned short _port;//MySQL的端口号3306
	string _username;//MySQL登录用户名
	string _password;//MySQL登陆密码
	string _dbname;//连接的数据库名称
	int _initSize;//连接池的初始连接量
	int _maxSize;//连接池的最大连接量
	int _maxIdleTime;//连接池最大空闲时间
	int _connectionTimeOut;//连接池获取连接的超过时间

	queue<MysqlConnection*> _connectionQue;//存MySQL连接的队列
	mutex _queueMutex; //维护连接队列的线程安全互斥锁
	atomic_int _connectionCnt;//记录连接所创connection连接的总数量
	condition_variable cv;//设置条件变量，用于连接生产线程和连接消费线程的通信
    
};