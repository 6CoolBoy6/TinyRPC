#include "zookeeperutil.h"
#include "mprpcapplication.h"
#include <semaphore.h>
#include <iostream>
#include <zookeeper/zookeeper.h>

/// 全局的watcher观察器，负责接收zkserver给zkclient的通知
/// @param zh zookeeper句柄
/// @param type 事件类型，例如ZOO_SESSION_EVENT
/// @param state zkclient的状态，例如ZOO_CONNECTED_STATE
/// @param path 触发事件的znode路径（如果有的话）
/// @param watcherCtx 传入的上下文，通常用于回调函数的额外信息
void global_watcher(zhandle_t *zh, int type,
                   int state, const char *path, void *watcherCtx)
{
    if (type == ZOO_SESSION_EVENT)  /// 回调的消息类型是和会话相关的消息类型
	{
		if (state == ZOO_CONNECTED_STATE)  /// 当zkclient与zkserver连接成功时
		{
			/// 获取zookeeper上下文中的信号量指针，并唤醒等待的线程
			sem_t *sem = (sem_t*)zoo_get_context(zh);
            sem_post(sem);
		}
	}
}

/// ZkClient构造函数，初始化成员变量m_zhandle为空指针
ZkClient::ZkClient() : m_zhandle(nullptr)
{
}

/// ZkClient析构函数，确保在对象销毁时释放zookeeper的资源
ZkClient::~ZkClient()
{
	/// m_zhandle 不为空，说明已经和 zkServer 进行连接了
    if (m_zhandle != nullptr)
    {
        zookeeper_close(m_zhandle); /// 关闭zookeeper句柄，释放资源
    }
}

/// 异步连接 zkserver 的方法 
void ZkClient::Start()
{
    /// 从配置文件中获取zookeeper的IP地址和端口号
    std::string host = MprpcApplication::GetInstance().GetConfig().Load("zookeeperip");
    std::string port = MprpcApplication::GetInstance().GetConfig().Load("zookeeperport");
    std::string connstr = host + ":" + port; /// 根据zookeeper要求，组装连接字符串 ip:port
    
	/*
	zookeeper_mt：多线程版本（异步）
	zookeeper的API客户端程序提供了三个线程：
		- API调用线程：由zookeeper_init函数启动
		- 网络I/O线程：负责网络通信，底层使用poll机制来处理I/O，因为客户端不需要高并发；会发送心跳消息；
		- watcher回调线程：用于处理来自zkserver的通知
	*/

	/**
	 * @brief 使用 zookeeper 提供的 zookeeper_init 来连接 zkserver
	 * @param connstr.c_str() 连接信息的字符串，包含IP和端口号
	 * @param global_watcher 回调函数，用于处理zkserver的事件
	 * @param 30000 会话超时时间，单位为毫秒
	 * @param nullptr 客户端会话ID，首次连接时为nullptr
	 * @param nullptr 回调函数的上下文，通常为空指针
	 * @param 0 Zookeeper初始化的标志，通常为0
	 * 
	 */
    m_zhandle = zookeeper_init(connstr.c_str(), global_watcher, 30000, nullptr, nullptr, 0);
    if (nullptr == m_zhandle) 
    {
        std::cout << "zookeeper_init error!" << std::endl;
        exit(EXIT_FAILURE); /// 初始化失败，退出程序
    }

	/// 初始化信号量，用于同步全局watcher和Start函数
    sem_t sem;
    sem_init(&sem, 0, 0); /// 初始化信号量，初始值为0
    zoo_set_context(m_zhandle, &sem); /// 给zookeeper句柄关联信号量，用于在回调中访问

    sem_wait(&sem); /// 等待信号量被唤醒，表明连接成功
    std::cout << "zookeeper_init success!" << std::endl;
}

/// @brief 创建znode节点的方法
/// @param path znode节点的路径
/// @param data znode节点的数据
/// @param datalen 数据的长度
/// @param state znode节点的创建类型，例如临时节点或永久节点
void ZkClient::Create(const char *path, const char *data, int datalen, int state)
{
    char path_buffer[128]; /// 用于存储创建成功后的znode路径
    int bufferlen = sizeof(path_buffer); /// 缓冲区长度
    int flag;
	
	/// 先判断path表示的znode节点是否存在，如果存在，就不再重复创建了
	flag = zoo_exists(m_zhandle, path, 0, nullptr); /// 检查znode节点是否存在
	if (ZNONODE == flag) /// 表示path的znode节点不存在
	{
		/// 创建指定path的znode节点
		flag = zoo_create(m_zhandle, path, data, datalen,
			&ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
		if (flag == ZOK)
		{
			std::cout << "znode create success... path:" << path << std::endl;
		}
		else
		{
			std::cout << "flag:" << flag << std::endl;
			std::cout << "znode create error... path:" << path << std::endl;
			exit(EXIT_FAILURE); /// 创建失败，退出程序
		}
	}
}

/// @brief 根据指定的path，获取znode节点的值
/// @param path znode节点的路径
/// @return 返回znode节点的数据，若获取失败则返回空字符串
std::string ZkClient::GetData(const char *path)
{
    char buffer[128]; /// 存储获取到的数据
	int bufferlen = sizeof(buffer); /// 缓冲区长度
	int flag = zoo_get(m_zhandle, path, 0, buffer, &bufferlen, nullptr); /// 获取znode数据
	if (flag != ZOK)
	{
		std::cout << "get znode error... path:" << path << std::endl;
		return ""; /// 获取失败，返回空字符串
	}
	else
	{
		return buffer; /// 返回获取到的数据
	}
}
