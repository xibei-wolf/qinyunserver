#include "tcpconnection.h"
#include "eventloop.h"
namespace net
{
    TcpConnection::TcpConnection(EventLoop *loop, int fd, int id,const InetAddress& peerAddr)
        : _loop(loop),
          _socket(new Socket(fd)),
          _channel(new Channel(loop, fd)),
          _state(kConnecting),
          _id(id),
          _peerAddr_(peerAddr)
    {
        _socket->setKeepAlive(true);
        _channel->setReadCallback(std::bind(&TcpConnection::handelRead, this, std::placeholders::_1));
        _channel->setWriteCallback(std::bind(&TcpConnection::handelWrite, this));
        _channel->setCloseCallback(std::bind(&TcpConnection::handelClose, this));
        _channel->setErrorCallback(std::bind(&TcpConnection::handelError, this));
    }
    void TcpConnection::send(const std::string & data){return send(data.c_str(),data.size());}
    void TcpConnection::send(const void *data, size_t len)
    {
        if (_loop->isInLoopThread())
        {
            sendInloop(data, len);
        }
        else
        {
            std::string str(static_cast<const char *>(data), len);
            void (TcpConnection::*fp)(std::string &) = &TcpConnection::sendInloop;
            _loop->runInLoop(std::bind(fp, this, str));
        }
    }
    void TcpConnection::sendInloop(std::string &data) { sendInloop(data.c_str(), data.size()); }
    void TcpConnection::sendInloop(const void *data, size_t len)
    {
        _loop->assertInLoopThread();
        bool errFlag = false;
        int32_t leftlen = len;
        int32_t writelen = 0;
        if (!_channel->isWriting() && _outputBuffer.readableBytes() == 0)
        {
            writelen = sockets::write(_socket->fd(), data, len);
            if (writelen >= 0)
            {
                leftlen -= writelen;
            }
            else
            {
                LOG_ERROR("sendInloop failed : fd is %d", _socket->fd());
                errFlag = true;
            }
        }
        if (leftlen > 0 && !errFlag)
        {
            _outputBuffer.append(static_cast<const char *>(data) + writelen, leftlen);
            if (!_channel->isWriting())
            {
                _channel->enableWriting();
            }
        }
    }

    void TcpConnection::forceClose()
    {
        _state = kDisconnecting;
        _loop->queueInLoop(std::bind(&TcpConnection::forceCloseInloop, shared_from_this()));
    }
    void TcpConnection::forceCloseInloop()
    {
        _loop->assertInLoopThread();
        if (_state == kConnected || _state == kDisconnecting)
        {
            handelClose();
        }
    }
    void TcpConnection::connectEstablished()
    {
        _loop->assertInLoopThread();
        assert(_state == kConnecting);
        _state = kConnected;
        _channel->tie(shared_from_this());
        _channel->enableReading();
        if (_onConnection)
            _onConnection(shared_from_this());
    }
    void TcpConnection::connectDestroyed()
    {
        _loop->assertInLoopThread();
        if (_state == kConnected)
        {
            _channel->disableAll();
            if (_onConnection)
                _onConnection(shared_from_this());
        }
        _channel->remove();
    }

    void TcpConnection::handelRead(Timestamp recvTime)
    {
        _loop->assertInLoopThread();
        int errNum;
        ssize_t n = _inputBuffer.readFd(_socket->fd(), &errNum);
        if (n == 0)
        {
            return handelClose();
        }
        else if (n < 0)
        {
            return handelError();
        }
        if (_onMessage)
            _onMessage(shared_from_this(), &_inputBuffer, recvTime);
    }
    void TcpConnection::handelWrite()
    {
        _loop->assertInLoopThread();
        if (!_channel->isWriting())
        {
            LOG_ERROR("connection FD %d is shutdown write", _socket->fd());
            return;
        }
        ssize_t n = sockets::write(_socket->fd(), _outputBuffer.peek(), _outputBuffer.readableBytes());
        if (n <= 0)
        {
            LOG_ERROR(" tcpconnection handlewrite erro");
            return;
        }
        _outputBuffer.retrieve(n);
        if (_outputBuffer.readableBytes() == 0)
        {
            _channel->disableWriting();
        }
    }
    void TcpConnection::handelClose()
    {
        _loop->assertInLoopThread();
        assert(_state == kConnected || _state == kDisconnecting);
        _state = kDisconnected;
        _channel->disableAll();
        TcpConnectionPtr gardThis(shared_from_this());
        if (_onConnection)
            _onConnection(gardThis);
        if (_onClose)
            _onClose(gardThis);
    }
    void TcpConnection::handelError()
    {
        _loop->assertInLoopThread();
        LOG_ERROR("coonnect has err");
    }
} // namespace net
