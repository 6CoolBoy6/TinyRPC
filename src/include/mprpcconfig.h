#pragma once

#include <unordered_map>
#include <string>

/// rpcserverip   rpcserverport    zookeeperip   zookeeperport
/// 框架读取配置文件 类
class MprpcConfig
{
public:
    /// 负责解析加载配置文件
    void LoadConfigFile(const char *config_file);
    /// 查询配置项信息
    std::string Load(const std::string &key);
private:
    /// @brief 以kv形式存储配置文件信息
    // 这里不需要考虑线程安全问题，因为框架只需要Init一次
    std::unordered_map<std::string, std::string> m_configMap;
    /// 去掉字符串前后的空格
    void Trim(std::string &src_buf);
};