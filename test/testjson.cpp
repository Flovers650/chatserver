#include "json.hpp"
#include<iostream>
#include<vector>
#include<map>
using namespace std;

using json = nlohmann::json;

//json序列化示例
void func1()
{
    json js;
    js["msg_type"] = 2;
    js["from"] = "zhang san";
    js["to"] = "li si";
    js["msg"] = "hello, what are you doing now?";
    cout<< js << endl;
}

int main()
{
    func1();
    return 0;
}