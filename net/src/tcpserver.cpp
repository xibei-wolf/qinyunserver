#include "tcpserver.h"
#include "socket.h"

namespace net
{
    void TcpServer::onNewConnection(int fd, InetAddress addr)
    {
        _baseloop->assertInLoopThread();
        EventLoop *ioloop = _pool.getNextLoop();
        auto id = _next_conn_id.fetch_add(1);
        TcpConnectionPtr conn(std::make_shared<TcpConnection>(ioloop, fd, id));
        conn->setConnectionCallback(_onConnection);
        conn->setMessageCallback(_onMessage);
        conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
        _connections.insert(std::make_pair(id, conn));
        ioloop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
    }

    void TcpServer::removeConnection(TcpConnectionPtr conn) { _baseloop->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn)); }
    void TcpServer::removeConnectionInLoop(TcpConnectionPtr conn)
    {
        _baseloop->assertInLoopThread();
        _connections.erase(conn->id());
        conn->loop()->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }

    TcpServer::TcpServer(EventLoop *loop, const InetAddress &addr)
        : _baseloop(loop),
          _acceptor(loop, addr),
          _pool(loop),
          _next_conn_id(1)
    {
        _acceptor.setNewConnectionCallback(std::bind(&TcpServer::onNewConnection, this,
                                                     std::placeholders::_1, std::placeholders::_2));
    }
    TcpServer::~TcpServer()
    {
        _baseloop->assertInLoopThread();
        for (auto &connection : _connections)
        {
            TcpConnectionPtr conn(connection.second);
            connection.second.reset();
            conn->loop()->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
        }
    }
    void TcpServer::setThreadNum(int count) { _pool.setThreadNum(count); }
    void TcpServer::setConnectionCallback(ConnectionCallback cb) { _onConnection = std::move(cb); }
    void TcpServer::setMessageCallback(MessageCallback cb) { _onMessage = std::move(cb); }

    void TcpServer::start()
    {
        _pool.start();
        _baseloop->runInLoop(std::bind(&Acceptor::listen, &_acceptor));
    }

} // namespace net
