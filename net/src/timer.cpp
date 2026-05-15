#include "timer.h"
#include "enum.h"
#include "eventloop.h"
#include <sys/timerfd.h>

namespace net
{
    std::atomic<int64_t> Timer::_numCreated = 0;

    int createTimerfd()
    {
        int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        if (fd < 0)
        {
            LOG_FATAL("timerfd_create error ");
        }
        return fd;
    }
    struct timespec howMuchTimeFromNow(Timestamp when)
    {
        Timestamp now = Timestamp::now();
        int64_t detal = when.microSecondsSinceEpoch() - now.microSecondsSinceEpoch();
        if (detal < 100)
        {
            detal = 100;
        }

        struct timespec retval;
        retval.tv_sec = detal / Timestamp::kMicroSecondsPerSecond;
        retval.tv_nsec = (detal % Timestamp::kMicroSecondsPerSecond) * 1000;
        return retval;
    }

    // 读取定时器时间数据（超时次数，8B） read
    void readTimerfd(int timerfd, Timestamp now)
    {
        int64_t val;
        ssize_t ret = ::read(timerfd, &val, sizeof(val));
        if (ret < 0)
        {
            LOG_ERROR("readTimerfd error ");
        }
    }
    // 重置定时器超时通知时间
    void resetTimerfd(int timerfd, Timestamp expiration)
    {
        struct itimerspec its;
        memset(&its, 0x00, sizeof(its));
        its.it_value = howMuchTimeFromNow(expiration);
        int ret = timerfd_settime(timerfd, 0, &its, nullptr);
        if (ret < 0)
        {
            LOG_ERROR("resetTimerfd ERROR: %s", strerror(errno));
        }
    }

    TimerQueue::TimerQueue(EventLoop *loop)
        : _loop(loop),
          _timerfd(createTimerfd()),
          _timerChannel(loop, _timerfd),
          _callingExpiredTimers(false)
    {
        _timerChannel.setReadCallback(std::bind(&TimerQueue::handleRead, this));
        _timerChannel.enableReading();
        LOG_DEBUG("new timerqueue %p", this);
    }

    TimerQueue::~TimerQueue()
    {
        _timerChannel.disableAll();
        _timerChannel.remove();
        ::close(_timerfd);
        for (auto &entry : _timers)
        {
            delete entry.second;
        }
    }
    // 1. 添加定时任务
    TimerId TimerQueue::addTimer(TimerCallback functor, Timestamp when, double interval)
    {
        Timer *timer = new Timer(std::move(functor), when, interval);
        _loop->runInLoop(std::bind(&TimerQueue::addTimerInLoop, this, timer));
        return TimerId(timer, timer->sequence());
    }

    // 2. 取消定时任务
    void TimerQueue::cancelTimer(TimerId tid)
    {
        _loop->runInLoop(std::bind(&TimerQueue::cancelTimerInLoop, this, std::move(tid)));
    }

    // 在loop线程中进行实际的添加/取消操作，保证成员操作的线程安全
    void TimerQueue::addTimerInLoop(Timer *timer)
    {
        _loop->assertInLoopThread();
        bool changed = insert(timer);
        if (changed)
        {
            resetTimerfd(_timerfd, timer->expired());
        }
    }
    void TimerQueue::cancelTimerInLoop(TimerId tid)
    {
        _loop->assertInLoopThread();
        assert(_timers.size() == _activeTimers.size());
        if (_activeTimers.find(tid._timer) != _activeTimers.end())
        {
            Entry entry(tid._timer->expired(), tid._timer);
            _timers.erase(entry);
            _activeTimers.erase(tid._timer);
            delete tid._timer;
        }
        else if (_callingExpiredTimers == true)
        {
            _cancelTimers.insert(tid._timer);
        }
        assert(_timers.size() == _activeTimers.size());
    }

    // 新增定时任务，返回标志：标识是否需要重新设置定时器超时时间
    bool TimerQueue::insert(Timer *timer)
    {
        bool change = false;
        if (_timers.empty() || timer->expired() < _timers.begin()->second->expired())
        {
            change = true;
        }
        auto ret1 = _timers.insert(Entry(timer->expired(), timer));
        assert(ret1.second);
        auto ret2 = _activeTimers.insert(timer);
        assert(ret2.second);
        assert(_timers.size() == _activeTimers.size());
        return change;
    }
    // 过期任务的处理
    // 获取所有过期的定时任务
    std::vector<Entry> TimerQueue::getExpired(Timestamp now)
    {
        std::vector<Entry> expired;
        Entry entry(now, (Timer *)UINTPTR_MAX);
        auto end = _timers.lower_bound(entry);
        assert(end == _timers.end() || now < end->first);
        std::copy(_timers.begin(), end, std::back_inserter(expired));
        _timers.erase(_timers.begin(), end);
        for (auto &timer : expired)
        {
            int n = _activeTimers.erase(timer.second);
            assert(n == 1);
        }
        assert(_timers.size() == _activeTimers.size());
        return expired;
    }
    // 重置过期的定时任务：针对当前过期的任务，找出重复性任务，重新添加到任务池中
    void TimerQueue::reset(std::vector<Entry> &list, Timestamp now)
    {
        for (auto &entry : list)
        {
            if (entry.second->repeated())
            {
                entry.second->restart(now);
                insert(entry.second);
            }
            else
            {
                delete entry.second;
            }
        }
        Timestamp t;
        if (!_timers.empty())
        {
            t = _timers.begin()->second->expired();
        }

        if (t.valid())
        {
            resetTimerfd(_timerfd, t);
        }
    }
    // 超时事件的处理
    void TimerQueue::handleRead()
    {
        _loop->assertInLoopThread();
        Timestamp now = Timestamp::now();

        readTimerfd(_timerfd, now);
        std::vector<Entry> expireds = getExpired(now);

        _callingExpiredTimers = true;

        _cancelTimers.clear();
        for (auto &entry : expireds)
        {
            entry.second->run();
        }
        _callingExpiredTimers = false;
        reset(expireds, Timestamp::now());
    }

} // namespace net
