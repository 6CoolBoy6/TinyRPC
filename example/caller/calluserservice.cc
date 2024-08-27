#include <iostream>
#include "mprpcapplication.h"
#include "user.pb.h"
#include "mprpcchannel.h"

int main(int argc, char **argv)
{
    /// 整个程序启动以后，想使用mprpc框架来享受rpc服务调用，一定需要先调用框架的初始化函数（只初始化一次）
    MprpcApplication::Init(argc, argv);

    /// 演示调用远程发布的rpc方法 Login
    fixbug::UserServiceRpc_Stub stub(new MprpcChannel());

    /// rpc方法的请求参数
    fixbug::LoginRequest request;
    request.set_name("zhang san");
    request.set_pwd("123456");

    /// rpc方法的响应
    fixbug::LoginResponse response;
    
    /// 发起rpc方法的调用  同步阻塞的rpc调用过程  MprpcChannel::callmethod
    MprpcController controller; /// 创建controller对象
    stub.Login(&controller, &request, &response, nullptr); /// RpcChannel->RpcChannel::callMethod 集中来做所有rpc方法调用的参数序列化和网络发送

    /// 判断是否调用成功，并进行相关处理
    if (controller.Failed())
    {
        /// RPC调用过程中出现错误，打印错误信息
        std::cout << controller.ErrorText() << std::endl;
    }
    else
    {
        /// 一次rpc调用完成，读调用的结果
        if (0 == response.result().errcode())
        {
            std::cout << "rpc login response success:" << response.sucess() << std::endl;
        }
        else
        {
            std::cout << "rpc login response error : " << response.result().errmsg() << std::endl;
        }
    }

    // /// 一次rpc调用完成，读调用的结果
    // if (0 == response.result().errcode())
    // {
    //     std::cout << "rpc login response success:" << response.sucess() << std::endl;
    // }
    // else
    // {
    //     std::cout << "rpc login response error : " << response.result().errmsg() << std::endl;
    // }

    /// 演示调用远程发布的rpc方法Register
    fixbug::RegisterRequest req;
    req.set_id(2000);
    req.set_name("mprpc");
    req.set_pwd("666666");
    fixbug::RegisterResponse rsp;

    /// 以同步的方式发起rpc调用请求，等待返回结果
    stub.Register(nullptr, &req, &rsp, nullptr); 

    /// 判断是否调用成功，并进行相关处理
    if (controller.Failed())
    {
        /// RPC调用过程中出现错误，打印错误信息
        std::cout << controller.ErrorText() << std::endl;
    }
    else
    {
        /// 一次rpc调用完成，读调用的结果
        if (0 == rsp.result().errcode())
        {
            std::cout << "rpc register response success:" << rsp.sucess() << std::endl;
        }
        else
        {
            std::cout << "rpc register response error : " << rsp.result().errmsg() << std::endl;
        }
    }

    // /// 一次rpc调用完成，读调用的结果
    // if (0 == rsp.result().errcode())
    // {
    //     std::cout << "rpc register response success:" << rsp.sucess() << std::endl;
    // }
    // else
    // {
    //     std::cout << "rpc register response error : " << rsp.result().errmsg() << std::endl;
    // }
    
    return 0;
}