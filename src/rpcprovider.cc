#include "rpcprovider.h"
#include "mprpcapplication.h"
#include "rpcheader.pb.h"
#include "logger.h"
#include "zookeeperutil.h"

/*
service_name =>  service描述   
                        =》 service* 记录服务对象
                        method_name  =>  method方法对象
json   protobuf
*/
/// 这里是框架提供给外部使用的，可以发布rpc方法的函数接口
void RpcProvider::NotifyService(google::protobuf::Service *service)
{
    ServiceInfo service_info;

    /// 获取了服务对象的描述信息
    const google::protobuf::ServiceDescriptor *pserviceDesc = service->GetDescriptor();
    /// 获取服务的名字
    std::string service_name = pserviceDesc->name();
    /// 获取服务对象service的方法的数量
    int methodCnt = pserviceDesc->method_count();

    /// std::cout << "service_name:" << service_name << std::endl;
    LOG_INFO("service_name:%s", service_name.c_str());

    /// 将所有的 服务名称 和 服务信息（服务对象、方法名、方法描述） 保存到 unordered_map 中
    for (int i=0; i < methodCnt; ++i)
    {
        /// 获取了服务对象指定下标的服务方法的描述（抽象描述） UserService   Login
        const google::protobuf::MethodDescriptor* pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        service_info.m_methodMap.insert({method_name, pmethodDesc});

        LOG_INFO("method_name:%s", method_name.c_str());
    }
    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
}

/// 启动rpc服务节点，开始提供rpc远程网络调用服务
/// 相当于启动了一个 epoll + 多线程的服务器
void RpcProvider::Run()
{
    /// 读取配置文件rpcserver的 ip 和 端口号
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    // c_str()将 string 转为 char* ，atoi 将 char* 转为 int
    uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    muduo::net::InetAddress address(ip, port);

    /// 创建TcpServer对象
    muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");

    /// 绑定连接回调和消息读写回调方法  分离了网络代码和业务代码，只需要关心 有没有新的用户连接 以及 已连接用户的读写事件
    server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1, 
            std::placeholders::_2, std::placeholders::_3));

    /// 设置muduo库的线程数量
    server.setThreadNum(4);

    /// 把当前rpc节点上要发布的 服务对象和方法名 全部注册到zk上面，让rpcClient可以从zk上发现服务
    /// session timeout 默认是 30s     zkclient 网络I/O线程在 1/3 * timeout 时间发送ping消息
    ZkClient zkCli;
    zkCli.Start(); /// 连接 zkServer
    /// service_name为永久性节点    method_name为临时性节点   zookeeper只支持一层一层的创建节点
    for (auto &sp : m_serviceMap) 
    {
        /// /service_name   比如创建 /UserServiceRpc 节点
        std::string service_path = "/" + sp.first;
        zkCli.Create(service_path.c_str(), nullptr, 0);
        for (auto &mp : sp.second.m_methodMap)
        {
            /// /service_name/method_name   比如创建 /UserServiceRpc/Login 存储当前这个rpc服务节点主机的ip和port
            std::string method_path = service_path + "/" + mp.first;
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);

            /// @param method_path_data 节点要存放的数据 比如 ip:port
            /// @param ZOO_EPHEMERAL 表示znode是一个临时性节点
            zkCli.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
        }
    }

    /// rpc服务端准备启动，打印信息
    std::cout << "RpcProvider start service at ip:" << ip << " port:" << port << std::endl;

    /// 启动网络服务
    server.start();
    /**
     * @brief 相当于 epoll_wait()，以阻塞方式等待远程连接；
     * @brief 有连接之后，进行 TCP 三次握手，然后会自动调用 OnConnection 回调函数
     */ 
    m_eventLoop.loop(); 
}

/// 新的 socket 连接回调  和 http 一样是短连接，服务端返回 rpc 方法的响应之后就断开连接了
void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if (!conn->connected())
    {
        /// 和rpc client的连接断开了 
        conn->shutdown();//相当于调用了 socket 的 close()，关闭 socket 文件描述符
    }
}

/*
在框架内部，RpcProvider和RpcConsumer协商好之间通信用的protobuf数据类型
service_name method_name args    定义proto的message类型，进行数据头的序列化和反序列化
                                 service_name method_name args_size
16UserServiceLoginzhang san123456   

消息格式为：
header_size(4个字节) + service_name 和 method_name + 参数大小 args_size       + 参数信息 args_str

10 "10"
10000 "1000000"
std::string   insert和copy方法 
*/
/// 已建立连接用户的读写事件回调 如果远程有一个rpc服务的调用请求，那么OnMessage方法就会响应
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn, 
                            muduo::net::Buffer *buffer, 
                            muduo::Timestamp)
{
    /// 网络上接收的远程rpc调用请求的字符流    Login args
    std::string recv_buf = buffer->retrieveAllAsString();

    /// 从字符流中读取前4个字节的内容
    uint32_t header_size = 0;
    recv_buf.copy((char*)&header_size, 4, 0);

    /// 根据header_size读取数据头的原始字符流，反序列化数据，得到rpc请求的详细信息
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if (rpcHeader.ParseFromString(rpc_header_str))
    {
        /// 数据头反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }
    else
    {
        /// 数据头反序列化失败
        std::cout << "rpc_header_str:" << rpc_header_str << " parse error!" << std::endl;
        return;
    }

    /// 获取rpc方法参数的字符流数据
    std::string args_str = recv_buf.substr(4 + header_size, args_size);

    /// 打印调试信息
    std::cout << "============================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl; 
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl; 
    std::cout << "service_name: " << service_name << std::endl; 
    std::cout << "method_name: " << method_name << std::endl; 
    std::cout << "args_str: " << args_str << std::endl; 
    std::cout << "============================================" << std::endl;

    /// 获取 service 对象和 method 对象
    auto it = m_serviceMap.find(service_name);
    if (it == m_serviceMap.end())
    {
        std::cout << service_name << " is not exist!" << std::endl;
        return;
    }

    auto mit = it->second.m_methodMap.find(method_name);
    if (mit == it->second.m_methodMap.end())
    {
        std::cout << service_name << ":" << method_name << " is not exist!" << std::endl;
        return;
    }

    google::protobuf::Service *service = it->second.m_service; /// 获取service对象  new UserService
    const google::protobuf::MethodDescriptor *method = mit->second; /// 获取method对象  Login

    /// 生成rpc方法调用的请求request和响应response参数
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if (!request->ParseFromString(args_str))
    {
        std::cout << "request parse error, content:" << args_str << std::endl;
        return;
    }
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    /// 给下面的method方法的调用，绑定一个Closure的回调函数
    /// 相当于是 bind 绑定器和 function 的应用
    google::protobuf::Closure *done = google::protobuf::NewCallback<RpcProvider, 
                                                                    const muduo::net::TcpConnectionPtr&, 
                                                                    google::protobuf::Message*>
                                                                    (this, 
                                                                    &RpcProvider::SendRpcResponse, 
                                                                    conn, response);

    /// 在框架上根据远端rpc请求，调用当前rpc节点上发布的方法
    /// new UserService().Login(controller, request, response, done)
    service->CallMethod(method, nullptr, request, response, done);
}

/// Closure的回调操作，用于序列化 rpc 的响应和网络发送
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message *response)
{
    std::string response_str;
    if (response->SerializeToString(&response_str)) /// response进行序列化
    {
        /// 序列化成功后，通过网络把rpc方法执行的结果发送会rpc的调用方
        conn->send(response_str);
    }
    else
    {
        std::cout << "serialize response_str error!" << std::endl; 
    }
    conn->shutdown(); /// 模拟http的短链接服务，由rpcprovider主动断开连接，方便给更多 rpc 调用方提供服务
}