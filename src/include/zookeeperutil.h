#pragma once

#include <semaphore.h>
#include <zookeeper/zookeeper.h>
#include <string>

/// 封装的zk客户端类
class ZkClient
{
public:
    ZkClient();
    ~ZkClient();
    /// zkclient启动连接zkserver
    void Start();
    /// 在zkserver上根据指定的path创建znode节点
    void Create(const char *path, const char *data, int datalen, int state=0); /// 默认是 0，代表永久性节点
    /// 根据参数指定的znode节点路径，获取znode节点的值
    std::string GetData(const char *path);
private:
    /// zk的客户端句柄，可以用来操作zkServer
    zhandle_t *m_zhandle;
};