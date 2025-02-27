#include "connectionpool.hpp"
#include "muduo/base/Logging.h"

//线程安全的懒汉单例函数接口
ConnectionPool* ConnectionPool::GetConnectionPool()
{
	static ConnectionPool pool;
	return &pool;
}

ConnectionPool::ConnectionPool()
{
    _ip = "127.0.0.1";
    _port = 3306;
    _username = "root";
    _password = "123456";
    _dbname = "chat";
    _initSize = 10;
    _maxSize = 1024;
    //最大空闲时间默认单位是秒
    _maxIdleTime = 60;
    //连接超时时间
    _connectionTimeOut = 100;

	//创建初始数量的连接
	for (int i = 0; i < _initSize; i++)
	{
		MysqlConnection* p = new MysqlConnection();
		p->connect(_ip, _port, _username, _password, _dbname);
		p->refreshAliveTime();
		_connectionQue.push(p);
		_connectionCnt++;
	}

	//启动一个新的线程，作为连接的生产者
	thread produce(bind(&ConnectionPool::produceConnectionTask,this));
	produce.detach();

	//启动一个新的定时线程，扫描超过maxIdleTime时间的空闲连接，进行对于的连接回收
	thread check(bind(&ConnectionPool::checkConnectionTask, this));
	check.detach();
}

void ConnectionPool::produceConnectionTask()
{
	for (;;)
	{
		unique_lock<mutex> lock(_queueMutex);
		while (!_connectionQue.empty())
		{
			cv.wait(lock);//队列不空，此处生产线程进入等待状态
		}

		//连接数量没有到达上限，继续创建新的连接
		if (_connectionCnt < _maxSize)
		{
			MysqlConnection* p = new MysqlConnection();
			p->connect(_ip, _port, _username, _password, _dbname);
			p->refreshAliveTime();
			_connectionQue.push(p);
			_connectionCnt++;
		}

		//通知消费者线程，可以消费连接了
		cv.notify_all();
	}
}

void ConnectionPool::checkConnectionTask()
{
	for (;;)
	{
		//通过sleep模拟定时效果
		this_thread::sleep_for(chrono::seconds(_maxIdleTime));

		//扫描整个队列，释放多余的连接
		unique_lock<mutex> lock(_queueMutex);
		if (_connectionCnt > _initSize)
		{
			MysqlConnection* p = _connectionQue.front();
			if (p->getAliveTime() >= _maxIdleTime*1000)
			{
				_connectionQue.pop();
				_connectionCnt--;
				delete p;
			}
			else
			{
				break;
			}
		}
	}
}

shared_ptr<MysqlConnection> ConnectionPool::getConnection()
{
	unique_lock<mutex> lock(_queueMutex);
	while (_connectionQue.empty())
	{
		if(cv_status::no_timeout== cv.wait_for(lock, chrono::milliseconds(_connectionTimeOut)))
		{
			if (_connectionQue.empty())
			{
				LOG_ERROR << "获取空闲连接超时...获取失败！";
				return nullptr;
			}
		}
	}

	shared_ptr<MysqlConnection> sp(_connectionQue.front(),
		[&](MysqlConnection* pcon) 
		{
			unique_lock<mutex> lock(_queueMutex);
			pcon->refreshAliveTime();
			_connectionQue.push(pcon);
		});
	_connectionQue.pop();
	
	if (_connectionQue.empty())
	{
		//谁消费了队列中的最后一个connection,谁负责通知一下生产者生产连接
		cv.notify_all();
	}
	return sp;
}