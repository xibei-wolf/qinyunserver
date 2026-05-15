#include "socket.h"
#include "enum.h"
#include <linux/tcp.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <linux/tcp.h>
#include <cstring>

namespace net
{
    namespace sockets
    {
        int createNoblockSocket()
        {
            int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
            if (sockfd < 0)
            {
                LOG_FATAL("createNoblockSocket failed!");
            }
            return sockfd;
        }
        int createBlockSocket()
        {
            int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
            if (sockfd < 0)
            {
                LOG_FATAL("createBlocksocket failed!");
            }
            return sockfd;
        }
        void bind(int sockfd, const struct sockaddr *addr)
        {
            int res = ::bind(sockfd, addr, sizeof(struct sockaddr_in));
            if (res < 0)
            {
                LOG_FATAL("socket bind erro ");
            }
        }
        int accept(int sockfd, struct sockaddr_in *addr)
        {
            socklen_t len = sizeof(sockaddr_in);
            int ret = ::accept4(sockfd, (struct sockaddr *)addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (ret < 0)
            {
                switch (errno)
                {
                case EAGAIN:       // 非阻塞场景下，没有新连接
                case ECONNABORTED: // 新连接异常
                case EINTR:        // 当前的阻塞操作被信号中断了
                case EPROTO:       // 协议错误
                case EPERM:        // 防火墙拦截
                case EMFILE:       // 文件描述符到达进程的限制上限
                    break;
                case EBADF:      // 坏的文件描述符
                case EFAULT:     // 地址参数无效
                case EINVAL:     // 参数无效
                case ENFILE:     // 系统描述符数量达到上限
                case ENOBUFS:    // 内存不足
                case ENOTSOCK:   // 描述符不是一个套接字描述符
                case EOPNOTSUPP: // 操作错误，描述符不是一个流式套接字
                    LOG_FATAL("accept new connection erro");
                default:
                    LOG_FATAL("accept new connection erro");
                }
            }
            return ret;
        }

        int connect(int sockfd, const struct sockaddr *addr)
        {
            int ret = ::connect(sockfd, addr, sizeof(struct sockaddr_in));
            if (ret < 0)
            {
                LOG_FATAL("connect server failed!");
            }
            return ret;
        }
        void listen(int sockfd)
        {
            int ret = ::listen(sockfd, LISTEN_COUNT);
            if (ret < 0)
            {
                LOG_FATAL("lisen err");
            }
        }
        ssize_t read(int fd, void *buf, size_t size)
        {
            return ::read(fd, buf, size);
        }
        ssize_t readv(int fd, struct iovec *vec, int count)
        {
            return ::readv(fd, vec, count);
        }
        ssize_t write(int fd, const void *buf, size_t size)
        {
            return ::write(fd, buf, size);
        }
        void close(int fd)
        {
            if (fd > 0)
            {
                ::close(fd);
            }
        }
        // 转换为：192.168.1.1:8080 inet_ntop
        void toIpPort(char *buf, size_t size, const struct sockaddr_in *addr)
        {
            memset(buf, 0x00, size);
            inet_ntop(AF_INET, &addr->sin_addr, buf, size);
            snprintf(buf + strlen(buf), size - strlen(buf), "%u", ntohs(addr->sin_port));
        }
        // 转为网络字节序地址结构数据 inet_pton
        void fromIpPort(const char *ip, uint16_t port, struct sockaddr_in *addr)
        {
            inet_pton(AF_INET, ip, &addr->sin_addr);
            addr->sin_port = htons(port);
            addr->sin_family = AF_INET;
        }

    } // namespace sockets

    // 初始化数据，INADDR_LOOPBACK / INADDR_ANY
    InetAddress::InetAddress(uint16_t port)
    {
        memset(&_addr, 0x00, sizeof(_addr));
        _addr.sin_family = AF_INET;
        _addr.sin_addr.s_addr = INADDR_ANY;
        _addr.sin_port = htons(port);
    }
    // 初始化数据
    InetAddress::InetAddress(const std::string ip, uint16_t port)
    {
        memset(&_addr, 0x00, sizeof(_addr));
        sockets::fromIpPort(ip.c_str(), port, static_cast<struct sockaddr_in *>(&_addr));
    }
    // 地址转字符串
    std::string InetAddress::toIpPort() const
    {
        char buff[64] = {0};
        sockets::toIpPort(buff, 64, &_addr);
        return buff;
    }
    // 获取地址数据
    const struct sockaddr *InetAddress::getSockAddr() const
    {
        return reinterpret_cast<const struct sockaddr *>(&_addr);
    }

    // 设置地址数据
    void InetAddress::setSockAddr(struct sockaddr_in addr)
    {
        _addr = addr;
    }

    void Socket::bind(const InetAddress &localaddr)
    {
        sockets::bind(_sockfd, localaddr.getSockAddr());
    }
    void Socket::listen()
    {
        sockets::listen(_sockfd);
    }
    int Socket::accept(InetAddress *peeraddr)
    {
        struct sockaddr_in addr;
        int ret = sockets::accept(_sockfd, &addr);
        peeraddr->setSockAddr(addr);
        return ret;
    }
    // IPPROTO_TCP， TCP_NODELAY
    void Socket::setTcpNoDelay(bool on)
    {
        int opt = on ? 1 : 0;
        setsockopt(_sockfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    }
    // SOL_SOCKET， SO_REUSEADDR
    void Socket::setReuseAddr(bool on)
    {
        int opt = on ? 1 : 0;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }
    // SOL_SOCKET， SO_REUSEPORT
    void Socket::setReusePort(bool on)
    {
        int opt = on ? 1 : 0;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    }
    // SOL_SOCKET， SO_KEEPALIVE
    void Socket::setKeepAlive(bool on)
    {
        int opt = on ? 1 : 0;
        setsockopt(_sockfd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
    }
}