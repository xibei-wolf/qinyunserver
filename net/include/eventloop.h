#pragma once

#include <sys/syscall.h>
#include <unistd.h>
#define gettid() syscall(SYS_gettid)
#define HAVE_GETTID 1

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <condition_variable>

#include "timer.h"
#include "timestamp.h"

namespace net
{
    class Channel;
    class Poller;
    class TimerId;

    static IgnoreSigPips ignore_pips;

    class EventLoop
    {
        using Functor = std::function<void()>;
        using ChannelList = std::vector<Channel *>;

    public:
        EventLoop();
        ~EventLoop();
        void loop();
        void runInLoop(Functor cb);
        void queueInLoop(Functor cb);
        void wakeup();
        void quit();
        void assertInLoopThread();
        bool isInLoopThread() const;
        void removeChannel(Channel *channel);
        void updateChannel(Channel *channel);
        bool hasChannel(Channel *channel);

        TimerId runAt(Timestamp time, TimerCallback cb);
        TimerId runAfter(double delay_sec, TimerCallback cb);
        TimerId runEvery(double interval_sec, TimerCallback cb);
        void cancel(TimerId timerId);

    private:
        // 针对_wakeupFd读取数据
        void handleRead();
        // 处理任务池中的任务
        void doPendingFunctors();

    private:
        pid_t _threadId;
        bool _looping;                // 状态描述：描述当前事件循环是否处于循环中
        std::atomic<bool> _quit;      // 退出标志，事件循环的循环条件
        bool _eventHandling;          // 状态描述：当前是否正在事件处理中
        bool _callingPendingFunctors; // 状态描述：当前事件循环是否处于执行任务池状态
        Timestamp _pollReturnTime;    // 获取poll监控返回时的时间

        std::unique_ptr<Poller> _poller;         // 事件监控器
        std::unique_ptr<TimerQueue> _timerQueue; // 定时器

        int _wakeupFd;                           // 事件描述符，主要用于唤醒事件监控
        std::unique_ptr<Channel> _wakeupChannel; // _wakeupFd对应的channel
        ChannelList _activeChannels;             // channel*数组，用于获取就绪的channel
        Channel *_currentActiveChannel;          // 从_activeChannels获取当前要处理的channel
        std::mutex _mutex;                       // 任务池的保护锁
        std::vector<Functor> _pendingFunctors;   // 任务池
    };

    class EventLoopThread
    {
    private:
        EventLoop *_loop;
        std::thread _thread;
        std::mutex _mutex;
        std::condition_variable _cond;

    private:
        void threadFunc();

    public:
        EventLoopThread();
        ~EventLoopThread();
        EventLoop *startLoop();
    };

    class EventLoopThreadPool
    {
    private:
        int _thread_num;
        int _next_idx;
        EventLoop *_baseloop;
        std::vector<std::unique_ptr<EventLoopThread>> _threads;
        std::vector<EventLoop *> _loops;

    public:
        EventLoopThreadPool(EventLoop *baseloop);
        ~EventLoopThreadPool();
        void setThreadNum(int count);
        void start();
        EventLoop *getNextLoop();
    };

}
