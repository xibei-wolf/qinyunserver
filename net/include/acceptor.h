#pragma once
#include <functional>

#include "socket.h"
#include "channel.h"

namespace net
{
    class EventLoop;
    using NewConnectionCallback = std::function<void(int, InetAddress)>;

    class Acceptor
    {
    public:
        Acceptor(EventLoop *loop, const InetAddress &addr);
        ~Acceptor();
        void setNewConnectionCallback(NewConnectionCallback cb);
        void listen();

    private:
        void handleRead(Timestamp recvTime);

    private:
        EventLoop *_loop;
        Socket _acceptsocket;
        Channel _acceptChannel;
        NewConnectionCallback _newConnCallback;
        int _idelFd;
    };

} // namespace net
