#pragma once

#include <sys/timerfd.h>
#include <cstdint>
#include <functional>
#include <atomic>
#include <assert.h>
#include <set>

#include "timestamp.h"
#include "channel.h"

namespace net
{
    using TimerCallback = std::function<void()>;
    class Timer;
    class EventLoop;
    class TimerId
    {
    public:
        TimerId(Timer *timer, int64_t seq) : _timer(timer), _sequence(seq) {}

    private:
        friend class TimerQueue;
        int64_t _sequence; // 定时任务唯一ID
        Timer *_timer;     // 定时任务对象指针
    };

    class Timer
    {

    public:
        Timer(TimerCallback cb, Timestamp expired, double interval)
            : _callback(std::move(cb)),
              _expired(expired),
              _repeated(interval > 0),
              _interval(interval),
              _sequence(_numCreated.fetch_add(1)) {}

        void run() { _callback(); }
        int64_t sequence() { return _sequence; }
        Timestamp expired() { return _expired; }
        bool repeated() { return _repeated; }
        double interval() { return _interval; }

        void restart(Timestamp now)
        {
            assert(repeated());
            _expired = addTime(now, _interval);
        }

    private:
        TimerCallback _callback;
        Timestamp _expired;
        bool _repeated;
        double _interval;
        int64_t _sequence;
        static std::atomic<int64_t> _numCreated;
    };

    int createTimerfd();
    struct timespec howMuchTimeFromNow(Timestamp when);
    // 读取定时器时间数据（超时次数，8B） read
    void readTimerfd(int timerfd, Timestamp now);
    // 重置定时器超时通知时间
    void resetTimerfd(int timerfd, Timestamp expiration);

    typedef std::pair<Timestamp, Timer *> Entry;
    typedef std::set<Entry> TimerList;

    class TimerQueue
    {

    public:
        TimerQueue(EventLoop *loop);
        ~TimerQueue();
        // 1. 添加定时任务
        TimerId addTimer(TimerCallback functor, Timestamp when, double interval);
        // 2. 取消定时任务
        void cancelTimer(TimerId tid);

    private:
        void addTimerInLoop(Timer *timer);
        void cancelTimerInLoop(TimerId tid);
        bool insert(Timer *timer);
        std::vector<Entry> getExpired(Timestamp now);
        void reset(std::vector<Entry> &list, Timestamp now);
        void handleRead();

    private:
        EventLoop *_loop;
        int _timerfd;
        Channel _timerChannel;
        TimerList _timers;
        std::atomic<bool> _callingExpiredTimers;
        std::set<Timer *> _activeTimers; // 为了快速找到定时任务，判断定时任务是否在任务池中
        std::set<Timer *> _cancelTimers; // 取消任务池
    };
}