#include "muduo/base/Logging.h"
#include "connectionpool.hpp"
#include "db.hpp"
#include <mysql/mysqld_error.h>

// 初始化数据库连接
Mysql::Mysql()
{
    _conn = ConnectionPool::GetConnectionPool()->getConnection();
}

//连接数据库
bool Mysql::connect()
{
    /*
    MYSQL *p = mysql_real_connect(_conn, server.c_str(), user.c_str(), 
                                password.c_str(), dbname.c_str(), 3306, nullptr, 0);
    if(p != nullptr)
    {
        //c和c++代码默认的编码字符ASCII，如果不设置，从Mysql上拉下来的中文显示？
        mysql_query(_conn, "set names gbk");
        LOG_INFO << "connect mysql success!";
    }
    else
    {
        LOG_INFO << "connect mysql fail!";
        LOG_INFO << mysql_error(_conn);     
    }
    return p;
    */
    if(_conn == nullptr)
    {
        return false;
    }
    return true;
}


//更新操作
bool Mysql::update(string sql)
{
    if(!_conn->update(sql))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "更新失败！";
        return false;
    }
    return true;
}

//查询操作
MYSQL_RES* Mysql::query(string sql)
{
    MYSQL_RES *res = _conn->query(sql);
    if(res == nullptr)
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "查询失败！";
        return nullptr;
    }
    return res;
}

//获取连接
MYSQL* Mysql::getConnection()
{
    return _conn->getConnection();
}
