#ifndef CHATSERVER_H
#define CHATSERVER_H

//#include "TcpServer.h"
#include "muduo/net/TcpServer.h"
using namespace muduo;
using namespace muduo::net;
#include <functional>

class ChatServer
{
public:
    ChatServer(EventLoop *loop,
            const InetAddress &addr,
            const std::string &name);
            
    void start();

private:
      //连接建立或者断开的回调
    void onConnection(const TcpConnectionPtr &conn);

    //可读写事件回调
    void onMessage(const TcpConnectionPtr &conn,
                Buffer *buf,
                Timestamp time);

    TcpServer server_;
    EventLoop* loop_;
    
};
    
#endif