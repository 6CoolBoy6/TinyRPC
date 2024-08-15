#include "test.pb.h"
#include <iostream>
#include <string>
using namespace fixbug;

int main()
{
    /// LoginResponse rsp;
    /// ResultCode *rc = rsp.mutable_result();
    /// rc->set_errcode(1);
    /// rc->set_errmsg("登录处理失败了");
    
    GetFriendListsResponse rsp;
    /// 不能直接使用 rsp.result()，因为 result 返回的是一个 const 对象
    /// 消息类型中的成员变量本身是一个对象，需要使用mutable_result 可改变的成员对象，返回的是指针，通过指针修改
    ResultCode *rc = rsp.mutable_result(); 
    rc->set_errcode(0);
  
    /// 列表类型 使用'add_' + '列表的成员变量名'，会新增一个列表中所管理的对象
    User *user1 = rsp.add_friend_list();
    user1->set_name("zhang san");
    user1->set_age(20);
    user1->set_sex(User::MAN);

    User *user2 = rsp.add_friend_list();
    user2->set_name("li si");
    user2->set_age(22);
    user2->set_sex(User::MAN);

    std::cout << rsp.friend_list_size() << std::endl;
    User *getuser = rsp.mutable_friend_list(0);
    std :: cout << getuser->name() << std :: endl;


    return 0;
}

int main1()
{
    /// 封装了login请求对象的数据
    LoginRequest req;
    req.set_name("zhang san");
    req.set_pwd("123456");

    /// 对象数据序列化 =》 char*
    std::string send_str;
    if (req.SerializeToString(&send_str))
    {
        std::cout << send_str.c_str() << std::endl;
    }

    /// 从send_str反序列化一个login请求对象
    LoginRequest reqB;
    if (reqB.ParseFromString(send_str))
    {
        std::cout << reqB.name() << std::endl;
        std::cout << reqB.pwd() << std::endl;
    }

    return 0;
}