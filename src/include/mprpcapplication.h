#pragma once  /// 防止头文件被重复包含

#include "mprpcconfig.h"
#include "mprpcchannel.h"
#include "mprpccontroller.h"

/// mprpc框架的基础类，负责框架的一些初始化操作
class MprpcApplication
{
public:
    static void Init(int argc, char **argv);
    static MprpcApplication& GetInstance(); /// 定义唯一的实例 单例设计模式
    static MprpcConfig& GetConfig();
private:
    static MprpcConfig m_config;

    MprpcApplication(){} /// 构造函数

    /// 拷贝构造相关的都删掉
    MprpcApplication(const MprpcApplication&) = delete;
    MprpcApplication(MprpcApplication&&) = delete;
};