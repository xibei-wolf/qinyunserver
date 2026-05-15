#include "acceptor.h"
#include "eventloop.h"
#include "enum.h"

#include <assert.h>

namespace net
{
    Acceptor::Acceptor(EventLoop *loop, const InetAddress &addr)
        : _loop(loop),
          _acceptsocket(sockets::createNoblockSocket()),
          _acceptChannel(loop, _acceptsocket.fd()),
          _idelFd(::open("/dev/null", O_CLOEXEC | O_RDONLY))
    {
        assert(_idelFd > 0);
        _acceptsocket.setReuseAddr(true);
        _acceptsocket.bind(addr);
        _acceptChannel.setReadCallback(std::bind(&Acceptor::handleRead, this, std::placeholders::_1));
    }

    Acceptor::~Acceptor()
    {
        ::close(_idelFd);
        _acceptChannel.disableAll();
        _acceptChannel.remove();
    }

    void Acceptor::handleRead(Timestamp recvTime)
    {
        _loop->assertInLoopThread();
        InetAddress peeraddr;
        int fd = _acceptsocket.accept(&peeraddr);
        if (fd >= 0)
        {
            if (_newConnCallback)
            {
                _newConnCallback(fd, peeraddr);
            }
            else
            {
                ::close(fd);
            }
        }
        else
        {
            LOG_ERROR("in Acceptor::handleRead");
            if (errno == EMFILE) // 文件描述符达到上限
            {
                ::close(_idelFd);
                _idelFd = _acceptsocket.accept(&peeraddr);
                ::close(_idelFd);
                _idelFd = ::open("/dev/null", O_CLOEXEC | O_RDONLY);
            }
            else if (errno != EAGAIN && errno != EINTR)
            {
                LOG_ERROR("Acceptor::handleRead accept failed");
            }
        }
    }

    void Acceptor::setNewConnectionCallback(NewConnectionCallback cb) { _newConnCallback = std::move(cb); }

    void Acceptor::listen()
    {
        _loop->assertInLoopThread();
        _acceptsocket.listen();
        _acceptChannel.enableReading();
    }

} // namespace net
