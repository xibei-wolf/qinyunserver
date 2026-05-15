#include "tcpclient.h"
#include "socket.h"
#include "eventloop.h"

namespace net
{
    TcpClient::TcpClient(EventLoop *loop, InetAddress addr)
        : _baseloop(loop),
          _srvAddr(addr) {}
    TcpClient::~TcpClient()
    {
        if (_connection)
        {
            _connection->forceClose();
        }
    }
    void TcpClient::connect()
    {
        int sockfd = sockets::createBlockSocket();
        int ret = sockets::connect(sockfd, _srvAddr.getSockAddr());
        int errNo = ret == 0 ? 0 : errno;
        switch (errNo)
        {
        case 0:           // 连接成功
        case EINTR:       // 阻塞连接过程被信号中断
        case EISCONN:     // 套接字已经连接成功
        case EINPROGRESS: // 套接字非阻塞的情况下连接服务器，返回正在连接中，需要监控可写事件，描述符可写了才代表连接成功
            _baseloop->runInLoop(std::bind(&TcpClient::newConnection, this, sockfd));
            break;
        case EAGAIN:        // 连接立即返回无法确定是否连接成功
        case EADDRINUSE:    // 当前连接绑定的地址被占用
        case EADDRNOTAVAIL: // 地址信息无效（通常在没有绑定地址内核会从空闲端口池绑定地址，但是端口池无空闲端口）
        case ECONNREFUSED:  // 连接被拒绝，通常指服务端没有对连接端口进行监听
        case ENETUNREACH:   // 网络不可达，通常指网络断开
        case ETIMEDOUT:     // 网络连接超时
            retry(sockfd);
            break;
        case EACCES:       // 权限错误
        case EPROTOTYPE:   // 套接字类型错误
        case ENOTSOCK:     // 描述符不是一个套接字描述符
        case EFAULT:       // 用户空间地址不够用了
        case EBADF:        // 文件描述符损坏
        case EALREADY:     // 非阻塞套接字上已有连接
        case EAFNOSUPPORT: // 不支持的地址域
        default:
            LOG_FATAL("连接服务器失败");
            break;
        }
    }
    TcpConnectionPtr TcpClient::connection()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond.wait(lock, [this]()
                   { return (bool)_connection; });
        return _connection;
    }

    void TcpClient::setConnectionCallback(ConnectionCallback cb) { _onConnection = std::move(cb); }
    void TcpClient::setMessageCallback(MessageCallback cb) { _onMessage = std::move(cb); }

    void TcpClient::retry(int fd)
    {
        sockets::close(fd);
        _baseloop->runAfter(3, std::bind(&TcpClient::retryInloop, this));
    }

    void TcpClient::retryInloop()
    {
        _baseloop->assertInLoopThread();
        connect();
    }
    void TcpClient::newConnection(int fd)
    {
        _baseloop->assertInLoopThread();
        TcpConnectionPtr conn(std::make_shared<TcpConnection>(_baseloop, fd, 0));
        conn->setConnectionCallback(_onConnection);
        conn->setMessageCallback(_onMessage);
        conn->setCloseCallback(std::bind(&TcpClient::removeConnection, this, std::placeholders::_1));
        conn->connectEstablished();
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _connection = conn;
            _cond.notify_all();
        }
    }
    void TcpClient::removeConnection(TcpConnectionPtr conn)
    {
        _baseloop->assertInLoopThread();
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _connection.reset();
        }
        conn->connectDestroyed();
    }
} // namespace net
