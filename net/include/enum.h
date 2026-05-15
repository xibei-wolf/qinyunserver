#pragma once

#include <cstdio>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>

#include "timestamp.h"

namespace net
{
    enum class LOG_LEVEL
    {
        TRACE = 0,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        NUM_LOG_LEVELS,
    };

    // tcpconncetion 连接状态
    enum State
    {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };

    class IgnoreSigPips
    {
    public:
        IgnoreSigPips() { signal(SIGPIPE, SIG_IGN); }
    };
}

// channel监控状态
namespace net
{
    const int KNew = -1;
    const int KAdded = 1;
    const int KDeleted = 2;
}

namespace net
{
    static const int KNonEvent = 0;
    static const int KReadEvent = EPOLLIN | EPOLLPRI;
    static const int KWriteEvent = EPOLLOUT;
}
namespace net
{
    class Channel;
    class EventLoop;
    class Poller;
}

namespace net
{
    // poller默认设置
    const int InitEpollTimeout = 1000;
    static const int KInitEventListSize = 16;
}

static net::LOG_LEVEL Def_Log_Level = net::LOG_LEVEL::INFO;

#define GET_SHORT_TID() (std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10000)

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LOG_INFO(format, ...)                                                                                      \
    if (net::LOG_LEVEL::INFO >= Def_Log_Level)                                                                     \
    {                                                                                                              \
        net::Timestamp now = net::Timestamp::now();                                                                \
        printf("[%s][INFO] [PID:%u TID:%04lu] [%s:%d] " format "\n",                                               \
               now.toFormattedString().c_str(), getpid(), GET_SHORT_TID(), __FILENAME__, __LINE__, ##__VA_ARGS__); \
    }

    
#define LOG_DEBUG(format, ...)                                                                                     \
    if (net::LOG_LEVEL::DEBUG >= Def_Log_Level)                                                                    \
    {                                                                                                              \
        net::Timestamp now = net::Timestamp::now();                                                                \
        printf("[%s][DEBUG] [PID:%u TID:%04lu] [%s:%d] " format "\n",                                              \
               now.toFormattedString().c_str(), getpid(), GET_SHORT_TID(), __FILENAME__, __LINE__, ##__VA_ARGS__); \
    }

#define LOG_ERROR(format, ...)                                                                                     \
    if (net::LOG_LEVEL::ERROR >= Def_Log_Level)                                                                    \
    {                                                                                                              \
        net::Timestamp now = net::Timestamp::now();                                                                \
        printf("[%s][ERROR] [PID:%u TID:%04lu] [%s:%d] " format "\n",                                              \
               now.toFormattedString().c_str(), getpid(), GET_SHORT_TID(), __FILENAME__, __LINE__, ##__VA_ARGS__); \
    }

#define LOG_FATAL(format, ...)                                                                                     \
    if (net::LOG_LEVEL::FATAL >= Def_Log_Level)                                                                    \
    {                                                                                                              \
        net::Timestamp now = net::Timestamp::now();                                                                \
        printf("[%s][FATAL] [PID:%u TID:%04lu] [%s:%d] " format "\n",                                              \
               now.toFormattedString().c_str(), getpid(), GET_SHORT_TID(), __FILENAME__, __LINE__, ##__VA_ARGS__); \
        abort();                                                                                                   \
    }

    
