#include "chatservice.hpp"
#include "public.hpp"
//#include "Logger.h"
#include "muduo/base/Logging.h"
#include <vector>
#include <iostream>
using namespace std::placeholders;

const string ROLE_CREATOR = "creator";
const string ROLE_NORMAL = "normal";


ChatService::ChatService()
{
    //用户基本业务管理相关事件处理回调注册
    _msgHandlerMap.insert({LOGIN_MSG, bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, bind(&ChatService::onechat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, bind(&ChatService::addfriend, this, _1, _2, _3)});

    //群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, bind(&ChatService::chatGroup, this, _1, _2, _3)});

    //连接redis服务器
    if(_redis.connect())
    {
        // 设置上报消息的回调
        _redis.init_notify_handler(bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

//获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}

//获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    auto it = _msgHandlerMap.find(msgid);
    if(it == _msgHandlerMap.end())
    {
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time)
        {
            LOG_ERROR << "msgid" << msgid << "can not find handler!";
            //LOG_ERROR("%d can not find handler!", msgid);
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

//处理登录业务
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"];
    string pwd = js["password"];
    json response;

    User user = _userModel.query(id);
    if(user.getId() == id && user.getPwd() == pwd)
    {
        if(user.getState() == "online")
        {
            //该用户已经登录，不允许重复登录
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "this account is using, input another!";
            conn->send(response.dump());
        }
        else
        {
            //登录成功
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            //用户登录成功后，向redis订阅channel(id)
            _redis.subscribe(id);

            user.setState("online");
            _userModel.updateState(user);

            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            //查询用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty())
            {
                response["offlinemsg"] = vec;
                //读取该用户的离线消息后，把该用户的所有离线消息删除掉
                _offlineMsgModel.remove(id);
            }

            //查询该用户的好友信息并返回
            vector<User> friendvec = _friendModel.query(id);
            if(!friendvec.empty())
            {
                vector<string> vec2;
                for(User &user : friendvec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            vector<Group> groupuserVec = _groupModel.queryGroup(id);
            if(!groupuserVec.empty())
            {
                vector<string> groupVec;
                for(auto &group : groupuserVec)
                {
                    json groupjs;
                    groupjs["id"] = group.getId();
                    groupjs["groupname"] = group.getName();
                    groupjs["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for(GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    groupjs["users"] = userV;
                    groupVec.push_back(groupjs.dump());
                }
                response["groups"] = groupVec;
            }
            conn->send(response.dump());
        }
    }
    else
    {
        //登录失败
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "id or password is invaild!";
        conn->send(response.dump());
    }
}

//处理注册业务
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    json response;

    if(_userModel.insert(user))
    {
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    } 
}

//处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"];

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if(it != _userConnMap.end())
        {
            _userConnMap.erase(userid);
        }
    }

    _redis.unsubscribe(userid);

    //更新用户的状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

//添加好友业务
void ChatService::addfriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int friendid = js["friendid"];
    int id = js["id"];

    if(_userModel.query(friendid).getId() != friendid)
    {
        js["msg"] = "Cannot find this id!";
        conn->send(js.dump());
        return;
    }

    _friendModel.insert(id, friendid);
}

//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"];
    string name = js["groupname"];
    string desc = js["groupdesc"];

    Group group(-1, name, desc);
    //存储新创建的群组信息
    if(_groupModel.createGroup(group))
    {
        //存储群组创建人信息
        _groupModel.addGroup(userid, group.getId(), ROLE_CREATOR);
    }
}

//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["userid"];
    int groupid = js["groupid"];
    _groupModel.addGroup(userid, groupid, ROLE_NORMAL);
}

//群发消息业务
void ChatService::chatGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["userid"];
    int groupid = js["groupid"];
    vector<int> user = _groupModel.queryGroupUsers(userid, groupid);

    if(!user.empty())
    {
        lock_guard<mutex> lock(_connMutex);
        for(int id : user)
        {
            auto it = _userConnMap.find(id);
            if(it != _userConnMap.end())
            {
                //转发群消息
                it->second->send(js.dump());
            }
            else
            {
                User user = _userModel.query(id);
                if(user.getState() == "online")
                {
                    _redis.publish(id, js.dump());
                }
                else
                {
                    //存储离线消息
                    _offlineMsgModel.insert(id, js.dump());
                }
            }
        }
    }
}

void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for(auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
            if(it->second == conn)
            {
                //从map表删除用户的连接消息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    _redis.unsubscribe(user.getId());

    //更新用户的状态信息
    if(user.getId() != -1)
    {
        user.setState("offline");   
        _userModel.updateState(user);
    }
}

//一对一聊天
void ChatService::onechat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["to"];

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end())
        {
            //toid在线，转发消息
            it->second->send(js.dump());
            return;
        }
    }

    User user = _userModel.query(toid);
    if(user.getState() == "online")
    {
        _redis.publish(toid, js.dump());
        return;
    }

    //toid不在线，存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
}

//服务器异常，业务重置方法
void ChatService::reset()
{
    //把online状态的用户，设置成offline
    _userModel.resetState();
}

void ChatService::handleRedisSubscribeMessage(int userid, string message)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it != _userConnMap.end())
    {
        it->second->send(message);
        return;
    }

    _offlineMsgModel.insert(userid, message);
}
