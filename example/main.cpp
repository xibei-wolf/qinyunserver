#include <iostream>

#include "timestamp.h"
#include "eventloop.h"
#include "tcpserver.h"

#include <assert.h>

using namespace std;

void onConnection(net::TcpConnectionPtr conn)
{
    if (conn->connected())
    {
        //LOG_INFO("新连接建立");
    }
    else
    {
        //LOG_INFO("连接关闭");
    }
}

void onMessage(net::TcpConnectionPtr conn, net::Buffer *buf, net::Timestamp rtime)
{
    std::string str = buf->retrieveAllAsString();
    // std::cout << str << std::endl;
    conn->send(str.data(), str.size());
    conn->forceClose();
}

int main()
{
    net::EventLoop baseloop;
    net::TcpServer server(&baseloop, net::InetAddress(9000));
    
    server.setThreadNum(4);
    server.setConnectionCallback(onConnection);
    server.setMessageCallback(onMessage);
    server.start();

    baseloop.loop();
    return 0;
}

// Buffer test
#if 0
#include "buffer.h"
int main()
{
    net::Buffer buf;
    assert(buf.readableBytes() == 0);    
    assert(buf.writableBytes() == 1024); 
    assert(buf.prependableBytes() == 8);

    buf.append("hello\n", 6); // 写入 hello\n
    assert(buf.readableBytes() == 6);
    buf.append("world\n", 6); // 写入 world\n
    assert(buf.readableBytes() == 12);

    cout << "=== 测试 getline() ===" << endl;
    auto line1 = buf.getline();
    assert(line1.has_value());        // 必须读到一行
    assert(*line1 == "hello\n");      // 内容必须正确
    assert(buf.readableBytes() == 6); // 读完后剩下6字节

    if (line1)
        cout << "第一行：" << *line1; // 输出 hello\n

    auto line2 = buf.getline();
    assert(line2.has_value()); // 必须读到第二行
    assert(*line2 == "world\n");
    assert(buf.readableBytes() == 0); // 读完必须空了
    
    if (line2)
        cout << "第二行：" << *line2; // 输出 world\n

    buf.append("12345", 5);
    string str = buf.retrieveAsString(5);
    assert(str == "12345");           // 读取内容必须正确
    assert(buf.readableBytes() == 0); // 读完必须空

    cout << "\n=== 测试读取5字节 ===" << endl;
    cout << "读取到：" << str << endl; // 输出 12345


    cout << "\n=== 测试自动扩容 ===" << endl;
    string big_data(2048, 'x'); // 2048个x，超过默认大小
    buf.append(big_data.c_str(), big_data.size());
    assert(buf.readableBytes() == 2048); // 可读必须=2048
    assert(buf.writableBytes() >= 0);     // 扩容后必须还有可写空间
    cout << "写入2048字节成功，当前可读大小："
         << buf.readableBytes() << endl;

    buf.retrieveAll();
    assert(buf.readableBytes() == 0);    // 清空后可读=0
    assert(buf.prependableBytes() == 8); // 读指针回到8
    cout << "\n清空后可读大小：" << buf.readableBytes() << endl;

    cout << "\n==== 所有测试通过！ ====\n";
    return 0;
}
#endif

// timer in eventloop test
#if 0
int main()
{
    net::EventLoop baseloop;
    auto timerid1 = baseloop.runAt(net::addTime(net::Timestamp::now(), 5), []
                                   { std::cout << "this is a 5s timer task" << std::endl; });
    auto timerid2 = baseloop.runAfter(2, []
                                      { std::cout << "this is a 2s timer task" << std::endl; });
    auto timerid3 = baseloop.runEvery(1, []
                                      { std::cout << "this is a 1s timer task" << std::endl; });

    baseloop.loop();
    return 0;
}
#endif

// 定时器测试
#if 0
#include "timer.h"

int main()
{
    net::EventLoop baseloop;
    net::TimerQueue timerqueue(&baseloop);
    LOG_DEBUG("timout when : %s", net::addTime(net::Timestamp::now(), 3).toFormattedString().c_str());
    auto tid1 = timerqueue.addTimer([]
                                    { std::cout << "this is a 5s timer task" << std::endl; }, net::addTime(net::Timestamp::now(), 3), 0);
    auto tid2 = timerqueue.addTimer([]
                                    { std::cout << "this is a 2s timer task" << std::endl; }, net::addTime(net::Timestamp::now(), 3), 0);
    auto tid3 = timerqueue.addTimer([]
                                    { std::cout << "this is a 1s loop timer task" << std::endl; }, net::addTime(net::Timestamp::now(), 3), 1);
    auto tid4 = timerqueue.addTimer([]
                                    { std::cout << "this is a 3s timer task" << std::endl; }, net::addTime(net::Timestamp::now(), 3), 0);

    timerqueue.cancelTimer(tid4);

    baseloop.loop();
    return 0;
}
#endif