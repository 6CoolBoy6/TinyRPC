#pragma once
#include <queue>
#include <thread>
#include <mutex> /// 引入互斥锁，用于线程间的互斥
#include <condition_variable> /// 引入条件变量，用于线程间的同步

/// 定义一个模板类 LockQueue，用于实现异步日志写入的日志队列
template<typename T>
class LockQueue
{
public:
    /// worker线程会调用该方法，将日志数据推送到日志队列中
    void Push(const T &data)
    {
        std::lock_guard<std::mutex> lock(m_mutex); /// 加锁，确保线程安全
        m_queue.push(data); /// 将日志数据推入队列
        m_condvariable.notify_one(); /// 通知等待队列的线程，有新数据可处理
    }

    /// 专门的一个日志线程调用该方法，从日志队列中取出数据并写入日志文件
    T Pop()
    {
        /// unique_lock 允许在等待条件变量通知时暂时释放锁，并在继续执行时重新获取锁
        std::unique_lock<std::mutex> lock(m_mutex); /// 加锁，确保线程安全
        while (m_queue.empty()) /// 如果队列为空
        {
            /// 日志队列为空时，线程进入等待状态，直到被唤醒
            m_condvariable.wait(lock);
        }

        T data = m_queue.front(); /// 取出队列头部的日志数据
        m_queue.pop(); /// 将取出的数据从队列中移除
        return data; /// 返回取出的日志数据
    }
private:
    std::queue<T> m_queue; /// 使用STL队列存储日志数据
    std::mutex m_mutex; /// 互斥锁，用于保护队列的访问
    std::condition_variable m_condvariable; /// 条件变量，用于线程间的同步
};
