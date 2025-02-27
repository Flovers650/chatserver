#ifndef CHATSERVICE_H
#define CHARSERVICE_H

#include <functional>
#include <mutex>
#include "json.hpp"
#include "redis.hpp"
#include "usermodel.hpp"
#include "groupmodel.hpp"
#include "offlinemessagemodel.hpp"
//#include "TcpServer.h"
#include "muduo/net/TcpServer.h"
#include "friendmodel.hpp"
#include <unordered_map>
using namespace std;
using namespace nlohmann;
using namespace muduo;
using namespace muduo::net;


//表示处理消息的事件回调方法类型
using MsgHandler = function<void(const TcpConnectionPtr &conn, json &js, Timestamp)>;

//聊天服务器业务类
class ChatService
{
public:
    //获取单例对象的接口函数
    static ChatService* instance();
    //处理登录业务
    void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //处理注册业务
    void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //一对一聊天业务
    void onechat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //添加好友业务
    void addfriend(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //创建群组业务
    void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //加入群组业务
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //群发消息业务
    void chatGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //处理注销业务
    void loginout(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //获取消息对应的处理器
    MsgHandler getHandler(int msgid);
    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);
    //服务器异常，业务重置方法
    void reset();
    //上报消息的回调
    void handleRedisSubscribeMessage(int userid, string message);

private:
    ChatService();

    //存储消息id和其对应的业务处理方法
    unordered_map<int, MsgHandler> _msgHandlerMap;

    //存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> _userConnMap;

    mutex _connMutex;
    
    //数据操作类对象
    UserModel _userModel; 
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;

    Redis _redis;
};

#endif

