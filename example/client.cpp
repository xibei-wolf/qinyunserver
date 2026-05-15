#include "tcpclient.h"
#include "eventloop.h"

#include <iostream>

void onConnection(net::TcpConnectionPtr conn)
{
    if (conn->connected())
    {
        LOG_DEBUG("新连接建立");
    }
    else
    {
        LOG_DEBUG("连接关闭");
    }
}

void onMessage(net::TcpConnectionPtr conn, net::Buffer *buf, net::Timestamp rtime)
{
    std::string str = buf->retrieveAllAsString();
    std::cout << str << std::endl;
    conn->send(str.data(), str.size());
    // conn->forceClose();
}

int main()
{
    net::EventLoopThread loopthread;

    {
        net::TcpClient client(loopthread.startLoop(), net::InetAddress("127.0.0.1", 9000));
        client.connect();
        client.setConnectionCallback(onConnection);
        client.setMessageCallback(onMessage);

        auto connection = client.connection();

        for (int i = 0; i < 10; i++)
        {
            std::string str = "hello world " + std::to_string(i) + "\n";
            connection->send(str.data(), str.size());
        }
        sleep(1);
    }

    sleep(1);

    return 0;
}