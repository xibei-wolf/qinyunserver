// ============================================================================
// BusinessServer.h — 青云志愿服务队管理系统 · 业务应用层服务器
//
// 职责：
//   1. 基于仿 Muduo 网络库封装 TCP 长连接服务
//   2. 解析 Header(4B 大端长度) + JSON Body 的自定义协议包
//   3. 根据 JSON 中的 "action" 字段路由到对应业务 Handler
//   4. 保证 Sub-Reactor 多线程环境下的线程安全响应
// ============================================================================

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <mysql/mysql.h>

// Muduo-like 网络库核心头文件
#include "eventloop.h"
#include "tcpserver.h"
#include "tcpconnection.h"
#include "buffer.h"
#include "socket.h"
#include "timestamp.h"

// JSON 解析库
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace qingyun
{

    // ============================================================================
    // 协议常量
    // ============================================================================
    namespace protocol
    {

        /// 协议头长度：uint32_t（网络字节序/大端序）的 body 长度
        constexpr size_t kHeaderSize = sizeof(uint32_t);

        /// 单包 Body 最大长度，防止恶意超大报文耗尽内存
        constexpr uint32_t kMaxBodyLen = 4 * 1024 * 1024; // 4 MB

    } // namespace protocol

    // ============================================================================
    // BusinessServer 类定义
    // ============================================================================
    class BusinessServer
    {
    public:
        enum RoleLevel {
            ROLE_TEACHER = 10,
            ROLE_CAPTAIN = 20,
            ROLE_MINISTER = 30,
            ROLE_MEMBER = 40
        };
        // ------------------------------------------------------------------
        // 构造 / 析构
        // ------------------------------------------------------------------

        /// @param loop       主 EventLoop（Main-Reactor），负责 Accept
        /// @param listenAddr 监听地址端口
        /// @param serverName TcpServer 内部标识名（影响日志输出）
        BusinessServer(net::EventLoop *loop,
                       const net::InetAddress &listenAddr,
                       std::string serverName = "QingYunServer");

        ~BusinessServer();

        // 禁止拷贝
        BusinessServer(const BusinessServer &) = delete;
        BusinessServer &operator=(const BusinessServer &) = delete;

        // ------------------------------------------------------------------
        // 服务器控制
        // ------------------------------------------------------------------
        /// 设置 Sub-Reactor 线程数（含 Main-Reactor）
        /// 传 0 表示单线程模式（Main-Reactor 兼做 IO 线程）
        void setThreadNum(int numThreads);

        /// 启动服务器——绑定端口并进入事件循环
        void start();
    public:
        // private:
        // ==================================================================
        // Muduo 框架回调（运行在 Sub-Reactor IO 线程中）
        // ==================================================================

        /// 连接建立 / 断开回调
        void onConnection(const net::TcpConnectionPtr &conn);

        /// 数据到达回调 —— 协议解包 + 业务分发的总入口
        /// @note 本函数运行在 Sub-Reactor IO 线程
        void onMessage(const net::TcpConnectionPtr &conn,
                       net::Buffer *buffer,
                       net::Timestamp timestamp);

        // ==================================================================
        // 协议层（粘包/半包处理）
        // ==================================================================

        /// 尝试从 Buffer 中提取一个完整的 JSON 报文
        /// @param  buffer      Muduo 输入缓冲区
        /// @param  outJsonBody [out] 成功时存入完整的 JSON Body 字符串
        /// @return true 提取成功，outJsonBody 有效；
        ///         false 数据不足（半包）或协议错误，buffer 中保留未消费的数据
        bool tryExtractPacket(net::Buffer *buffer, std::string &outJsonBody);

        // 业务路由（Router）
        using HandlerFunc = std::function<void(const net::TcpConnectionPtr &conn, const nlohmann::json &)>;

        /// action → handler 映射表
        std::unordered_map<std::string, HandlerFunc> router_;

        /// 根据 action 字段分发请求
        void dispatchAction(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);

        // 业务 Handler（供 Router 分发调用）
        void handleLogin(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        void handleUploadSchedule(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);

        void handleFilterMembers(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        void handleConfirmAssign(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        void handleGetActivities(const net::TcpConnectionPtr &conn, const json &jsonObj);
        void handleAddActivity(const net::TcpConnectionPtr &conn, const json &jsonObj);
        void handleGetDepartments(const net::TcpConnectionPtr &conn, const json &jsonObj);

        void handleGetManagementActivities(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);

        
        void handleGetMembers(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        void handleBulkRegister(const net::TcpConnectionPtr &conn, const json &jsonObj);
        
        void handleDeleteActivity(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        void handleUpdateActivity(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        void handleApplyActivity(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        void handleLeaveActivity(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        void handleDeleteMember(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        void handleGetClassTemplate(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        void handleSetTermStart(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        void handleGetTermStart(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        void handleGetAssignedMembers(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        void handleGetTimeSlotAnalytics(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        
        void handleBatchDeleteMembers(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        void handleBatchAssignMembers(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        void handleBatchApplyClassTemplate(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);
        
        void handleGetRegisteredClasses(const net::TcpConnectionPtr &conn, const nlohmann::json &jsonObj);  
        void handleApproveApplication(const net::TcpConnectionPtr &conn, const json &jsonObj);
        void handleSettleActivity(const net::TcpConnectionPtr &conn, const json &jsonObj);
        void handleUnknownAction(const net::TcpConnectionPtr &conn, const json &jsonObj);



        // ==================================================================
        // 响应发送工具（线程安全）
        // ==================================================================

        /// 发送 JSON 响应报文 [4B 大端长度][JSON]
        /// @note Muduo 的 conn->send() 内部已保证跨线程安全。
        ///       即使本函数在非 IO 线程调用，框架也会将 send 任务
        ///       投递到对应连接的 IO 线程执行（EventLoop::queueInLoop）。
        void sendResponse(const net::TcpConnectionPtr &conn,const nlohmann::json &responseJson);

        /// 发送错误响应的便捷封装
        void sendError(const net::TcpConnectionPtr &conn, int errorCode,const std::string &errorMessage);

        bool isAuthorized(const json &jsonObj, int minRoleLevel);
        // JSON 字段安全提取
        /// 校验 JSON 对象是否包含指定类型字段
        static bool validateField(const nlohmann::json &jsonObj,
                                  const std::string &fieldName,
                                  nlohmann::json::value_t expectedType);
        // 数据成员
        net::EventLoop *mainLoop_;               // 主 Reactor（不拥有所有权）
        std::unique_ptr<net::TcpServer> server_; // Muduo TcpServer 实例
        MYSQL *m_mysql = nullptr;

        private:
            std::string m_cachedTermStartDate;
            void loadSystemConfig(); // 在构造函数中调用
            std::string escapeString(const std::string &str); // SQL 字符串转义
            int getCurrentWeek(); // 实时计算当前周数
            void rebuildUserSchedules(int userId); // 重建用户课表位图缓存
    };

} // namespace qingyun
