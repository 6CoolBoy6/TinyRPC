#include "logger.h"
#include <time.h>
#include <iostream>

/// 获取日志的单例
Logger& Logger::GetInstance()
{
    static Logger logger;
    return logger;
}

Logger::Logger()
{
    /// @brief 启动专门的写日志线程，将lockqueue缓冲区的日志写到磁盘中
    std::thread writeLogTask([&](){
        for (;;)
        {
            /// 获取当前的日期，然后取日志信息，写入相应的日志文件当中 a+（追加读写模式）
            time_t now = time(nullptr);
            tm *nowtm = localtime(&now);

            /// 文件名
            char file_name[128];
            /// @param tm_year 是一个相对于1900的年份偏移量，因此需要加1900得到当前年份
            /// @param tm_mon 从0开始计算的，即0表示1月，因此需要加1得到正确的月份
            sprintf(file_name, "%d-%d-%d-log.txt", nowtm->tm_year+1900, nowtm->tm_mon+1, nowtm->tm_mday);

            /// 以追加并读写的方式打开文件
            FILE *pf = fopen(file_name, "a+");
            if (pf == nullptr)
            {
                std::cout << "logger file : " << file_name << " open error!" << std::endl;
                exit(EXIT_FAILURE);
            }

            /// @brief 从 lockqueue 缓冲区中取出信息
            std::string msg = m_lckQue.Pop();

            /// 格式：时分秒=>日志信息
            char time_buf[128] = {0};
            sprintf(time_buf, "%d:%d:%d =>[%s] ", 
                    nowtm->tm_hour, 
                    nowtm->tm_min, 
                    nowtm->tm_sec,
                    (m_loglevel == INFO ? "info" : "error"));
            msg.insert(0, time_buf);
            msg.append("\n");

            /// 将信息写入文件中pf
            fputs(msg.c_str(), pf);
            fclose(pf);
        }
    });
    /// 设置分离线程，相当于守护线程
    writeLogTask.detach();
}

/// 设置日志级别 
void Logger::SetLogLevel(LogLevel level)
{
    m_loglevel = level;
}

/// 写日志， 把日志信息写入lockqueue缓冲区当中
void Logger::Log(std::string msg)
{
    m_lckQue.Push(msg);
}