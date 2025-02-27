#include "chatserver.hpp"
#include "chatservice.hpp"
#include "muduo/base/Logging.h"
#include "json.hpp"
using namespace std::placeholders;
using namespace nlohmann;


ChatServer::ChatServer(EventLoop *loop,
    const InetAddress &addr,
    const std::string &name)
    : loop_(loop)
    , server_(loop, addr, name)
    {
        server_.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));
        server_.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

        server_.setThreadNum(4);
    }

void ChatServer::start()
{
    server_.start();
}

void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    if(!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

//可读写事件回调
void ChatServer::onMessage(const TcpConnectionPtr &conn,
                Buffer *buffer,
                Timestamp time)
    {
        string msg = buffer->retrieveAllAsString();
        //数据的反序列化
        json js = json::parse(msg);
        //达到的目的：完全理解网络模块的代码和业务模块的代码
        //通过js["msgid"] 获取=》业务handler=》conn js time
        auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
        msgHandler(conn, js, time);
    }