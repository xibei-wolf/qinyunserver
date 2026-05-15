#pragma once

#include <memory>
#include <functional>
#include <any>
#include <assert.h>

#include "buffer.h"
#include "timestamp.h"
#include "enum.h"
#include "socket.h"
#include "channel.h"

namespace net
{
    class EventLoop;
    class TcpConnection;

    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
    using ConnectionCallback = std::function<void(TcpConnectionPtr)>;
    using MessageCallback = std::function<void(TcpConnectionPtr, Buffer *, Timestamp)>;
    using CloseCallback = std::function<void(TcpConnectionPtr)>;

    class TcpConnection : public std::enable_shared_from_this<TcpConnection>
    {

    public:
        TcpConnection(EventLoop *loop, int fd, int id);
        ~TcpConnection() { assert(_state == kDisconnected); }

        int id() { return _id; }
        bool connected() const { return _state == State::kConnected; }
        bool disconnected() const { return _state == State::kDisconnected; }
        EventLoop *loop() { return _loop; }
        Buffer *inputBuffer() { return &_inputBuffer; }
        Buffer *outputBuffer() { return &_outputBuffer; }
        // 上下文函数
        const std::any &context() const { return _context; }
        std::any &mutableContext() { return _context; }
        void setContext(std::any context) { _context = std::move(context); }
        // 设置回调函数
        void setConnectionCallback(ConnectionCallback cb) { _onConnection = std::move(cb); }
        void setMessageCallback(MessageCallback cb) { _onMessage = std::move(cb); }
        void setCloseCallback(CloseCallback cb) { _onClose = std::move(cb); }

        void send(const void *data, size_t len);
        void forceClose();
        void connectEstablished();
        void connectDestroyed();

    private:
        void sendInloop(const void *data, size_t len);
        void sendInloop(std::string &data);
        void forceCloseInloop();
        void handelRead(Timestamp recvTime);
        void handelWrite();
        void handelClose();
        void handelError();

    private:
        const int64_t _id;
        State _state;
        EventLoop *_loop;
        std::unique_ptr<Socket> _socket;
        std::unique_ptr<Channel> _channel;
        Buffer _inputBuffer;
        Buffer _outputBuffer;
        std::any _context;
        ConnectionCallback _onConnection;
        MessageCallback _onMessage;
        CloseCallback _onClose;
    };

} // namespace net
