#include "mprpcchannel.h"
#include <string>
#include "rpcheader.pb.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include "mprpcapplication.h"
#include "mprpccontroller.h"
#include "zookeeperutil.h"

/*
RPC请求的数据格式为：
header_size + service_name method_name + args_size + args
*/
/// 所有通过stub代理对象调用的rpc方法，都走到这里了，统一做rpc方法调用的数据数据序列化和网络发送 
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                                google::protobuf::RpcController* controller, 
                                const google::protobuf::Message* request,
                                google::protobuf::Message* response,
                                google::protobuf:: Closure* done)
{
    const google::protobuf::ServiceDescriptor* sd = method->service();
    std::string service_name = sd->name(); /// service_name
    std::string method_name = method->name(); /// method_name

    /// 获取参数的序列化字符串长度 args_size
    uint32_t args_size = 0;
    std::string args_str;

    /// 参数序列化
    if (request->SerializeToString(&args_str))
    {
        args_size = args_str.size();
    }
    else
    {
        controller->SetFailed("serialize request error!");
        return;
    }
    
    /// 定义rpc的请求header
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    /// 请求头序列化
    if (rpcHeader.SerializeToString(&rpc_header_str))
    {
        header_size = rpc_header_str.size();
    }
    else
    {
        controller->SetFailed("serialize rpc header error!"); /// 保存错误状态信息
        return;
    }

    /// 1. 组装 序列化之后待发送的 rpc 请求的字符串 send_rpc_str
    std::string send_rpc_str;
    send_rpc_str.insert(0, std::string((char*)&header_size, 4)); /// header_size
    send_rpc_str += rpc_header_str; /// rpcheader（service_name + method_name + 参数大小 args_size）
    send_rpc_str += args_str; /// args

    /// 打印调试信息
    std::cout << "============================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl; 
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl; 
    std::cout << "service_name: " << service_name << std::endl; 
    std::cout << "method_name: " << method_name << std::endl; 
    std::cout << "args_str: " << args_str << std::endl; 
    std::cout << "============================================" << std::endl;

    /// 2. 使用tcp编程，完成rpc方法的远程调用（调用端不需要高并发，简单的TCP编程即可）

    /// 2.1 创建 socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        char errtxt[512] = {0};
        sprintf(errtxt, "create socket error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    /// 读取配置文件rpcserver的信息
    /// std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    /// uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    /// rpc调用方想调用service_name的method_name服务，需要查询zk上该服务所在的host信息

    /// 2.2 从 zookeeper 中查找 rpcserver 的 ip 地址 和 端口号
    ZkClient zkCli;
    zkCli.Start();

    /// 组装 znode 节点路径  比如 /UserServiceRpc/Login
    std::string method_path = "/" + service_name + "/" + method_name;

    /// 127.0.0.1:8000
    std::string host_data = zkCli.GetData(method_path.c_str());

    /// 出错信息处理
    if (host_data == "")
    {
        controller->SetFailed(method_path + " is not exist!");
        return;
    }

    /// 以 : 来分隔 将ip和端口号分别取出
    int idx = host_data.find(":");
    if (idx == -1)
    {
        controller->SetFailed(method_path + " address is invalid!");
        return;
    }
    std::string ip = host_data.substr(0, idx);
    uint16_t port = atoi(host_data.substr(idx+1, host_data.size()-idx).c_str()); 

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET; // 地址家族
    server_addr.sin_port = htons(port); // 本地字节序转为网络字节序
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    /// 2.3 连接rpc服务节点
    if (-1 == connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr)))
    {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "connect error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    /// 2.4 发送rpc请求
    if (-1 == send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0))
    {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "send error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    /// 2.5 接收rpc请求的响应值
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if (-1 == (recv_size = recv(clientfd, recv_buf, 1024, 0)))
    {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "recv error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    /**
     * @bug 出现BUG，recv_buf中遇到\0后面的数据就存不下来了，导致反序列化失败
     * 原因：
     * 应该是字符串构造函数中，通过打印可知 recv_buf 内容为 "\n\000\020\001"， 
     * 所以将recv_buf的数组下标为 0 到 recv_size 个字符赋值给response_str时，遇到 "\0" (结束运算符)，就自动结束了；
     * 导致使用gdb打印 "p response_str" 结果为 "\n"；没能正确将recv_buf的内容赋值给response_str
     */
    /// std::string response_str(recv_buf, 0, recv_size); 
    /// if (!response->ParseFromString(response_str)) /// 将字符串 response_str 中的内容解析（反序列化）为 response 对象的字段值


    /// 2.6 反序列化rpc调用的响应数据
    /// response->ParseFromArray(recv_buf, recv_size) 将字节数组（recv_buf）中的数据反序列化为 protobuf 消息对象（response）中的字段
    /**
     * @param recv_buf 这是一个指向接收到的数据缓冲区的指针。该缓冲区通常存储了从网络或其他 I/O 源接收到的字节流
     * @param recv_size 这是一个整数，表示缓冲区 recv_buf 中数据的大小（即字节数）
     */
    if (!response->ParseFromArray(recv_buf, recv_size))
    {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "parse error! response_str:%s", recv_buf);
        controller->SetFailed(errtxt);
        return;
    }

    close(clientfd);
}