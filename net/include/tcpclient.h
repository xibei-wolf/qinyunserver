#pragma once
#include "tcpconnection.h"

#include <condition_variable>
#include <mutex>

namespace net
{
    class InetAddress;
    class TcpClient
    {
    public:
        TcpClient(EventLoop *loop, InetAddress addr);
        ~TcpClient();
        void connect();
        TcpConnectionPtr connection();
        void setConnectionCallback(ConnectionCallback cb);
        void setMessageCallback(MessageCallback cb);

    private:
        void retry(int fd);
        void retryInloop();
        void newConnection(int fd);
        void removeConnection(TcpConnectionPtr conn);

    private:
        std::mutex _mutex;
        std::condition_variable _cond;
        InetAddress _srvAddr;
        EventLoop *_baseloop;
        TcpConnectionPtr _connection;
        ConnectionCallback _onConnection;
        MessageCallback _onMessage;
    };

} // namespace net
