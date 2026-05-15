#pragma once

#include <memory>
#include <functional>
#include "enum.h"
#include "timestamp.h"

namespace net
{
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    class Channel
    {
    public:
        Channel(EventLoop *loop, int fd);
        ~Channel();
        void setReadCallback(ReadEventCallback cb) { _readCallback  = std::move(cb); }
        void setWriteCallback(EventCallback cb)    { _writeCallback = std::move(cb); }
        void setCloseCallback(EventCallback cb)    { _closeCallback = std::move(cb); }
        void setErrorCallback(EventCallback cb)    { _errorCallback = std::move(cb); }

        void remove();

        void enableReading()  {_events |= KReadEvent;update();}
        void disableReading() {_events &= ~KReadEvent;   update();}
        void enableWriting()  {_events |= KWriteEvent;update();}
        void disableWriting() {_events &= ~KWriteEvent;update();}
        void disableAll()     {_events = KNonEvent;   update();}

        bool isReading() const { return _events == KReadEvent; }
        bool isWriting() const { return _events == KWriteEvent; }
        bool isNoneEvent() const { return _events == KNonEvent; }

        int fd() const { return _fd; }
        int index() { return _index; }
        int events() const { return _events; }

        void setindex(int state) { _index = state; }
        void setRevent(int event) { _revents = event; }

        EventLoop *ownerloop() { return _loop; }

        void tie(const std::shared_ptr<void> &obj){_tie = obj;_tied = true;}

        void handleEvent(Timestamp receiveTime)
        {
            if (_tied)
            {
                if (_tie.lock()) handleEventWithGuard(receiveTime);
            }
            else
            {
                handleEventWithGuard(receiveTime);
            }
        }

    private:
        void update();
        void handleEventWithGuard(Timestamp receiveTime)
        {
            _eventHandling = true;
            if (_revents & EPOLLHUP && !(_revents & EPOLLIN))
            {
                if (_closeCallback) _closeCallback();
            }
            if (_revents & (EPOLLIN | EPOLLPRI) || _revents & EPOLLRDHUP)
            {
                if (_readCallback) _readCallback(receiveTime);
            }
            if (_revents & EPOLLOUT)
            {
                if (_writeCallback) _writeCallback();
            }
            if (_revents & EPOLLERR)
            {
                if (_errorCallback) _errorCallback();
            }
            _eventHandling = false;
        }

    private:
        EventLoop *_loop;         // 当前channel挂在哪个loop中进⾏事件监控
        int _fd;                  // 当前channel对应的描述符
        uint32_t _events;         // 当前channel所监控的事件
        uint32_t _revents;        // 当前channel所触发的事件
        int _index;               // 当前channel的状态
        std::weak_ptr<void> _tie; // 观察者模式的⼀种另类使⽤，⽤于观察指定的对象存在性
        bool _tied;
        bool _eventHandling; // 当前channel是否处于事件处理中
        bool _addedToLoop;   // 当前channel对象是否已经被添加到loop中

        // 不同事件的处理回调函数
        ReadEventCallback _readCallback;
        EventCallback _writeCallback;
        EventCallback _closeCallback;
        EventCallback _errorCallback;
    };
}