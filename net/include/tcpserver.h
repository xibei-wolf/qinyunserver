#pragma once
#include "tcpconnection.h"
#include "eventloop.h"
#include "acceptor.h"

#include <map>
#include <atomic>

namespace net
{
    class InetAddress;
    class TcpServer
    {
    public:
        TcpServer(EventLoop *loop, const InetAddress &addr,const std::string& nameArg="");
        ~TcpServer();
        void setThreadNum(int count);
        void setConnectionCallback(ConnectionCallback cb);
        void setMessageCallback(MessageCallback cb);
        void start();

    private:
        void onNewConnection(int fd, InetAddress addr);
        void removeConnection(TcpConnectionPtr conn);
        void removeConnectionInLoop(TcpConnectionPtr conn);

    private:
        EventLoop *_baseloop;
        EventLoopThreadPool _pool;
        Acceptor _acceptor;
        std::atomic<int64_t> _next_conn_id;
        std::unordered_map<int64_t, TcpConnectionPtr> _connections;
        ConnectionCallback _onConnection;
        MessageCallback _onMessage;
    };

} // namespace net
