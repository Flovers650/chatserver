#include "json.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>
#include <ctime>
#include <atomic>
#include <semaphore.h>
#include "group.hpp"
#include "user.hpp"
#include "public.hpp"
using namespace std;
using json = nlohmann::json;

static int readthreadnum = 0;

//记录当前系统登录的用户信息
User g_currentUser;
//记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
//记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

//控制聊天页面程序
bool isMainMenuRunning = false;

//用于读写线程之间的通信
sem_t rwsem;
//记录登录状态
atomic_bool g_isLoginSuccess{false};

//接受线程
void readTaskHandler(int fd);
//获取系统时间 (聊天信息需要添加时间信息)
string getCurrentTime();
//主聊天页面程序
void mainMenu(int fd);
//显示当前登录成功用户的基本信息
void showCurrentUserData();

//聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char **argv)
{
    if(argc < 3)
    {
        cerr << "command invalid example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    //解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    //创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(clientfd == -1)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    } 

    //填写client需要连接的server信息ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(port);

    //client和server进行连接
    if(connect(clientfd, (sockaddr*)&server, sizeof(server)) == -1)
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    // 连接服务器成功，启动接受子线程
    thread readTask(readTaskHandler, clientfd);
    readTask.detach();

    //初始化读写线程通信用的信号量
    sem_init(&rwsem, 0, 0);

    //main线程用于接受用户输入，负责发送数据
    for(;;)
    {
        // 显示首页面菜单 登录，注册，退出
        cout << "========================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "========================" << endl;
        cout << "choice: ";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读取缓冲区残留的回车

        switch(choice)
        {
        case 1: // login 业务
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "userid:";
            cin >> id;
            cin.get();
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();
        
            g_isLoginSuccess = false;    
            int len = send(clientfd, request.c_str(), request.size() + 1, 0);
            if(len == -1)
            {
                cerr << "send login msg error: " << request << endl;
            }

            sem_wait(&rwsem); //等待信号量，由子线程处理完登录的响应消息，通知这里
            
            if(g_isLoginSuccess)
            {
                isMainMenuRunning = true;
                mainMenu(clientfd);
            }
            //进入聊天主菜单页面
            isMainMenuRunning = true;
            mainMenu(clientfd);              
        }
        break;
        case 2: //register 业务
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username:";
            cin.getline(name, 50);
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();
           
            int len = send(clientfd, request.c_str(), request.size() + 1, 0);
            if(len == -1)
            {
                cerr << "send reg msg error: " << request << endl;
            }

            sem_wait(&rwsem);
        }   
        break;
        case 3: // quit业务
            close(clientfd);
            sem_destroy(&rwsem);
            exit(0);
        default:
            cerr << "invalid input!" << endl;
            break;
        }

    }

    return 0;
}

// 处理注册的相应逻辑
void doRegResponse(json &responsejs)
{
    json responsejs = json::parse(buffer);
    if(responsejs["errno"] != 0) // 注册失败
    {
        cerr << "name is already exist, register error!" << endl;
    }
    else // 注册成功
    {
        cout << "name register success, userid is " << responsejs["id"]
             << ", do not forget it!" << endl;
    }
}

// 处理登录的响应逻辑
void doLoginResponse(json &responsejs)
{
    if(responsejs["errno"] != 0) // 登录失败
    {
        cerr << responsejs["errmsg"] << endl;
        g_isLoginSuccess = false;
    }
    else // 登录成功
    {
        //记录当前用户的id和name
        g_currentUser.setId(responsejs["id"]);
        g_currentUser.setName(responsejs["name"]);
    
        //记录当前用户的好友列表信息
        if(responsejs.contains("friends"))
        {
            g_currentUserFriendList.clear();
            vector<string> vecf = responsejs["friends"];
            for(string &str : vecf)
            {
                json js = json::parse(str);
                User user;
                user.setId(js["id"]);
                user.setName(js["name"]);
                user.setState(js["state"]);
                g_currentUserFriendList.push_back(user);
            }
        }
    
        //记录当前用户的群组列表信息
        if(responsejs.contains("groups"))
        {
            g_currentUserGroupList.clear();
            vector<string> vecg = responsejs["groups"];
            for(string &groupstr : vecg)
            {
                json groupjs = json::parse(groupstr);
                Group group;
                group.setId(groupjs["id"]);
                group.setName(groupjs["groupname"]);
                group.setDesc(groupjs["groupdesc"]);
    
                vector<string> vecu = groupjs["users"];
                for(string &userstr : vecu)
                {
                    json js = json::parse(userstr);
                    GroupUser user;
                    user.setId(js["id"]);
                    user.setName(js["name"]);
                    user.setRole(js["role"]);
                    user.setState(js["state"]);
                    group.getUsers().push_back(user);
                }
                g_currentUserGroupList.push_back(group);
            }
        }
    
        //显示登录用户的基本信息
        showCurrentUserData();
    
        //显示当前用户的离线消息，个人聊天信息或者群组消息
        if(responsejs.contains("offlinemsg"))
        {
            vector<string> vec = responsejs["offlinemsg"];
            for(string &str : vec)
            {
                json js = json::parse(str);
                if(ONE_CHAT_MSG == js["msgid"])
                {
                    cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                        << " said: " << js["msg"].get<string>() << endl;
                }
                else
                {
                    cout << "groupmsg: " << js["time"].get<string>() << " [" << js["groupid"] << "]" << " [" << js["groupname"].get<string>() << "]" <<" [" << js["userid"] << "]" << js["username"].get<string>() 
                        << " said: " << js["msg"].get<string>() << endl;
                }
            }
        }

        g_isLoginSuccess = true;
    }
}

//接收线程
void readTaskHandler(int fd)
{
    for(;;)
    {
        char buffer[1024] = {0};
        int len = recv(fd, buffer, 1024, 0);
        if(len == -1 || len == 0)
        {
            close(fd);
            exit(-1);
        }

        json js = json::parse(buffer);
        if(ONE_CHAT_MSG == js["msgid"])
        {
            cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                << " said: " << js["msg"].get<string>() << endl;
            continue;
        }
        else
        {
            cout << "groupmsg: " << js["time"].get<string>() << " [" << js["groupid"] << "]" << " [" << js["groupname"].get<string>() << "]" <<" [" << js["userid"] << "]" << js["username"].get<string>() 
                << " said: " << js["msg"].get<string>() << endl;
            continue;
        }

        if(LOGIN_MSG_ACK == js["msgid"])
        {
            doLoginResponse(js); // 处理登录响应的业务逻辑
            sem_post(&rwsem); // 通知主线程，登录结果处理完成
            continue;
        }

        if(REG_MSG_ACK == js["msgid"])
        {
            doRegResponse(js); //处理登录响应的业务逻辑
            sem_post(&rwsem); // 通知主线程，注册结果处理完成
            continue;
        }
    }
}

void help(int fd = 0, string str = "");
void chat(int, string);
void addfriend(int, string);
void creategroup(int, string);
void addgroup(int, string);
void groupchat(int, string);
void loginout(int, string);

//系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令, 格式help"},
    {"chat", "一对一聊天, 格式chat:friendid:message"},
    {"addfriend", "添加好友, 格式addfriend:friendid"},
    {"creategroup", "创建群组, 格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组, 格式addgroup:groupid"},
    {"groupchat", "群聊, 格式groupchat:groupid:message"},
    {"loginout", "注销, 格式loginout"}
};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout},
};

//主聊天页面程序
void mainMenu(int fd)
{
    help();
    char buffer[1024] = {0};
    while(isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandBuf(buffer);
        string command;
        int idx = commandBuf.find(':');
        if(idx == -1)
        {
            command = commandBuf;
        }
        else
        {
            command = commandBuf.substr(0, idx);
        }

        auto it = commandHandlerMap.find(command);
        if(it == commandHandlerMap.end())
        {
            cerr << "invalid input command" << endl;
            continue;
        }

        //调用相应命令的事件处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        it->second(fd, commandBuf.substr(idx + 1, commandBuf.size() - idx -1));
    }
}

void help(int, string)
{
    cout << "show command list" << endl;
    for(auto &p : commandMap)
    {
        cout << p.first << ":" << p.second << endl;
    }
    cout << endl;
}

void addfriend(int fd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(fd, buffer.c_str(), buffer.size() + 1, 0);
    if(len == -1)
    {
        cerr << "send addfriend msg error -> " << buffer << endl;
    }
}

void chat(int fd, string str)
{
    int idx = str.find(':');
    if(idx == -1)
    {
        cerr << "chat command invalid!" << endl;
        return;
    }
    
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["to"] = atoi(str.substr(0, idx).c_str());
    js["msg"] = str.substr(idx + 1, str.size() - idx -1);
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(fd, buffer.c_str(), buffer.size() + 1, 0);  
    if(len == -1)
    {
        cerr << "send chat msg error -> " << buffer << endl;
    }  
}

void creategroup(int fd, string str)
{
    int idx = str.find(':');
    if(idx == -1)
    {
        cerr << "creategroup command invalid!" << endl;
        return;
    }

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = str.substr(0, idx);
    js["groupdesc"] = str.substr(idx + 1, str.size() - idx - 1);
    string buffer = js.dump();

    int len = send(fd, buffer.c_str(), buffer.size() + 1, 0);  
    if(len == -1)
    {
        cerr << "send creategroup msg error -> " << buffer << endl;
    }  
}

void addgroup(int fd, string str)
{
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["userid"] = g_currentUser.getId();
    js["groupid"] = atoi(str.c_str());
    string buffer = js.dump();

    int len = send(fd, buffer.c_str(), buffer.size() + 1, 0);  
    if(len == -1)
    {
        cerr << "send addgroup msg error -> " << buffer << endl;
    }  
}

void groupchat(int fd, string str)
{
    int idx = str.find(':');
    if(idx == -1)
    {
        cerr << "groupchat command invalid!" << endl;
        return;
    }

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["userid"] = g_currentUser.getId();
    js["username"] = g_currentUser.getName();
    js["time"] = getCurrentTime();
    js["groupid"] = atoi(str.substr(0, idx).c_str());

    for(auto g : g_currentUserGroupList)
    {
        if(g.getId() == js["groupid"])
        {
            js["groupname"] = g.getName();
            break;
        }
    }
    js["msg"] = str.substr(idx + 1, str.size() - idx - 1);

    string buffer = js.dump();

    int len = send(fd, buffer.c_str(), buffer.size() + 1, 0);  
    if(len == -1)
    {
        cerr << "send groupchat msg error -> " << buffer << endl;
    }  
}

void loginout(int fd, string str)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(fd, buffer.c_str(), buffer.size() + 1, 0);  
    if(len == -1)
    {
        cerr << "send loginout msg error -> " << buffer << endl;
    }  
    else
    {
        isMainMenuRunning = false;
    }
}

//显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout << "====================login user====================" << endl;
    cout << "current login user => id:" << g_currentUser.getId() << " " << "name:" << g_currentUser.getName() << endl;
    cout << "------------------friend list---------------------" << endl;
    if(!g_currentUserFriendList.empty())
    {
        for(User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }

    cout << "------------------group list----------------------" << endl;
    if(!g_currentUserGroupList.empty())
    {
        for(Group &group : g_currentUserGroupList)
        {
            cout << "---" << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for(GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState() << " " << user.getRole() << endl;
            }
        }
    }

    cout << "==================================================" << endl;
}

//获取系统时间 (聊天信息需要添加时间信息)
string getCurrentTime()
{
    auto time = chrono::system_clock::to_time_t(chrono::system_clock::now());
    struct tm *ptm = localtime(&time);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
        (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
        (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return string(date); 
}   
