#pragma once

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>

namespace net
{
    namespace sockets
    {
        static const int LISTEN_COUNT = 1024;

        int createNoblockSocket();
        int createBlockSocket();
        void bind(int sockfd, const struct sockaddr *addr);
        int accept(int sockfd, struct sockaddr_in *addr);
        int connect(int sockfd, const struct sockaddr *addr);
        void listen(int sockfd);
        ssize_t read(int fd, void *buf, size_t size);
        ssize_t readv(int fd, struct iovec *vec, int count);
        ssize_t write(int fd, const void *buf, size_t size);
        void close(int fd);
        // 转换为：192.168.1.1:8080 inet_ntop
        void toIpPort(char *buf, size_t size, const struct sockaddr_in *addr);
        // 转为网络字节序地址结构数据 inet_pton
        void fromIpPort(const char *ip, uint16_t port, struct sockaddr_in *addr);

    } // namespace sockets

    class InetAddress
    {
    public:
        // 初始化数据，INADDR_LOOPBACK / INADDR_ANY
        explicit InetAddress(uint16_t port = 0);
        // 初始化数据
        InetAddress(const std::string ip, uint16_t port);
        // 地址转字符串
        std::string toIpPort() const;
        // 获取地址数据
        const struct sockaddr *getSockAddr() const;
        // 设置地址数据
        void setSockAddr(struct sockaddr_in addr);

    private:
        struct sockaddr_in _addr;
    };

    class Socket
    {
    public:
        explicit Socket(int sockfd) : _sockfd(sockfd) {}
        ~Socket() { sockets::close(_sockfd); }
        int fd() { return _sockfd; }

        void bind(const InetAddress &localaddr);
        void listen();
        int accept(InetAddress *peeraddr);
        // IPPROTO_TCP， TCP_NODELAY
        void setTcpNoDelay(bool on);
        // SOL_SOCKET， SO_REUSEADDR
        void setReuseAddr(bool on);
        // SOL_SOCKET， SO_REUSEPORT
        void setReusePort(bool on);
        // SOL_SOCKET， SO_KEEPALIVE
        void setKeepAlive(bool on);

    private:
        const int _sockfd;

    };
}