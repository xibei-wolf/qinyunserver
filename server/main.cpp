#include <iostream>
#include "BusinessServer.h"
#include "eventloop.h"
#include "socket.h"

// 全局业务服务器指针
qingyun::BusinessServer* g_businessServer = nullptr;

// 1. 严格对齐你 PDF 第 1 页的 onConnection 签名
void onConnection(net::TcpConnectionPtr conn)
{
    if (g_businessServer) {
        g_businessServer->onConnection(conn);
    }
}

// 2. 严格对齐你 PDF 第 1 页的 onMessage 签名（去掉引用的 &）
void onMessage(net::TcpConnectionPtr conn, net::Buffer* buffer, net::Timestamp rtime)
{
    std::cout << "[Debug] Underlying Muduo loop successfully pushed bytes!" << std::endl;
    if (g_businessServer) {
        g_businessServer->onMessage(conn, buffer, rtime);
    }
}
int main()
{
    std::cout << "[QingYun Server] Starting backbone routing engine..." << std::endl;

    net::EventLoop loop;
    net::InetAddress listenAddr(9000);

    net::TcpServer server(&loop, listenAddr);

    g_businessServer = new qingyun::BusinessServer(&loop, listenAddr);

    server.setThreadNum(3);
    server.setConnectionCallback(onConnection);
    server.setMessageCallback(onMessage);

    server.start();
    loop.loop();

    return 0;
}