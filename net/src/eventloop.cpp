#include "eventloop.h"
#include "channel.h"
#include "poller.h"
#include <assert.h>
#include <sys/eventfd.h>

namespace net
{

    int createEventFd()
    {
        int fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (fd < 0)
        {
            LOG_FATAL("eventfd create failed %s ", strerror(errno));
        }
        return fd;
    }

    void writeEventFd(int fd)
    {
        uint64_t val = 1;
        ssize_t res = ::write(fd, &val, sizeof(val));
        if (res < 0)
        {
            LOG_ERROR("eventfd write err %s ", strerror(errno));
        }
    }

    void readEventFd(int fd)
    {
        uint64_t val = 1;
        ssize_t res = ::read(fd, &val, sizeof(val));
        if (res < 0)
        {
            LOG_ERROR("eventfd read err %s ", strerror(errno));
        }
    }

    EventLoop::EventLoop()
        : _looping(false),
          _quit(false),
          _eventHandling(false),
          _callingPendingFunctors(false),
          _threadId(::gettid()),
          _poller(Poller::newDefaultPoller(this)),
          _timerQueue(new TimerQueue(this)),
          _wakeupFd(createEventFd()),
          _wakeupChannel(new Channel(this, _wakeupFd)),
          _pollReturnTime{}
    {
        _wakeupChannel->setReadCallback(std::bind(&EventLoop::handleRead, this));
        _wakeupChannel->enableReading();
        LOG_DEBUG("eventloop created");
    }

    EventLoop ::~EventLoop()
    {
        assert(_looping == false);
        if (_poller->hasChannel(_wakeupChannel.get()))
        {
            _wakeupChannel->disableAll();
            _wakeupChannel->remove();
        }
        ::close(_wakeupFd);
    }

    // 开始事件循环：1. 开始描述符事件监控 ， 2. 处理任务池的任务
    void EventLoop::loop()
    {
        _looping = true;
        _quit = false;
        while (!_quit)
        {
            _activeChannels.clear();
            Timestamp now = _poller->poll(InitEpollTimeout, &_activeChannels);
            _currentActiveChannel = nullptr;

            _eventHandling = true;
            for (auto &channel : _activeChannels)
            {
                channel->handleEvent(now);
            }
            _eventHandling = false;

            doPendingFunctors();
        }

        _looping = false;
    }
    // 设置_quit标志，如果当前处于事件监控中，则唤醒循环，退出循环
    void EventLoop::quit()
    {
        _quit = true;
        if (!isInLoopThread())
        {
            wakeup();
        }
    }
    // 如果当前就在loop所在线程中，就直接执行，否则将任务压入任务池中
    void EventLoop::runInLoop(Functor cb)
    {
        if (isInLoopThread())
        {
            cb();
        }
        else
        {
            queueInLoop(std::move(cb));
        }
    }
    // 将任务压入任务池中,唤醒事件监控
    void EventLoop::queueInLoop(Functor cb)
    {
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _pendingFunctors.push_back(std::move(cb));
        }
        if (_callingPendingFunctors || !isInLoopThread())
        {
            wakeup(); // 唤醒事件监控
        }
    }
    // 唤醒事件监控：向_wakeupFd写入数据

    void EventLoop::wakeup()
    {
        writeEventFd(_wakeupFd);
    }

    void EventLoop::assertInLoopThread()
    {
        assert(_threadId == ::gettid());
    }

    bool EventLoop::isInLoopThread() const
    {
        if (_threadId == ::gettid())
        {
            return true;
        }
        return false;
    }

    void EventLoop::removeChannel(Channel *channel)
    {
        assertInLoopThread();
        _poller->removeChannel(channel);
    }
    void EventLoop::updateChannel(Channel *channel)
    {
        assertInLoopThread();
        _poller->updateChannel(channel);
    }

    bool EventLoop::hasChannel(Channel *channel)
    {
        assertInLoopThread();
        return _poller->hasChannel(channel);
    }
    void EventLoop::handleRead()
    {
        readEventFd(_wakeupFd);
    }

    void EventLoop::doPendingFunctors()
    {
        _callingPendingFunctors = true;
        std::vector<Functor> functors;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            std::swap(_pendingFunctors, functors);
        }
        for (auto &fun : functors)
        {
            fun();
        }

        _callingPendingFunctors = false;
    }
    TimerId EventLoop::runAt(Timestamp time, TimerCallback cb)
    {
        return _timerQueue->addTimer(std::move(cb), time, 0);
    }
    TimerId EventLoop::runAfter(double delay_sec, TimerCallback cb)
    {
        return _timerQueue->addTimer(std::move(cb), addTime(Timestamp::now(), delay_sec), 0);
    }
    TimerId EventLoop::runEvery(double interval_sec, TimerCallback cb)
    {
        return _timerQueue->addTimer(std::move(cb), addTime(Timestamp::now(), interval_sec), interval_sec);
    }
    void EventLoop::cancel(TimerId timerId)
    {
        _timerQueue->cancelTimer(timerId);
    }

    EventLoopThread::EventLoopThread()
        : _loop(nullptr),
          _thread(&EventLoopThread::threadFunc, this) {}
    EventLoopThread::~EventLoopThread()
    {
        _loop->quit();
        _thread.join();
    }
    void EventLoopThread::threadFunc()
    {
        EventLoop loop;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _loop = &loop;
            _cond.notify_all();
        }
        loop.loop();
    }

    EventLoop *EventLoopThread::startLoop()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond.wait(lock, [this]()
                   { return _loop; });
        return _loop;
    }

    EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseloop)
        : _baseloop(baseloop),
          _thread_num(0),
          _next_idx(0) {}
    EventLoopThreadPool::~EventLoopThreadPool() {}
    
    void EventLoopThreadPool::setThreadNum(int count) { _thread_num = count; }
    
    void EventLoopThreadPool::start()
    {
        _baseloop->assertInLoopThread();
        for (int i = 0; i < _thread_num; i++)
        {
            _threads.push_back(std::make_unique<EventLoopThread>());
            _loops.push_back(_threads.back()->startLoop());
        }
    }
    EventLoop *EventLoopThreadPool::getNextLoop()
    {
        _baseloop->assertInLoopThread();
        if (_thread_num == 0)
            return _baseloop;
        EventLoop *retval = _loops[_next_idx];
        _next_idx = (_next_idx + 1) % _loops.size();
        return retval;
    }

} // namespace net
