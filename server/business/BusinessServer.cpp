// ============================================================================
// BusinessServer.cpp — 青云志愿服务队管理系统 · 业务应用层服务器标准实现
// ============================================================================

#include "BusinessServer.h"
#include "TimeConverter.h"
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <limits.h>
#include <libgen.h>
#include <unistd.h>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#endif

using nlohmann::json;

namespace qingyun
{

    std::string getExecutableDir() {
        char buffer[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
        if (len != -1) {
            buffer[len] = '\0';
            // dirname 会修改传入的字符串，所以先复制一份
            std::string exePath(buffer);
            return std::string(dirname(&exePath[0]));
        }
        // 失败时返回当前目录（降级方案）
        return ".";
    }

    std::string getCanonicalConfigPath() {
        std::string rawPath = getExecutableDir() + "/../sql/db.conf";
        char* resolved = realpath(rawPath.c_str(), nullptr);
        if (resolved) {
            std::string result(resolved);
            free(resolved);
            return result;
        }
        return rawPath; // 降级返回原始拼接路径
    }

    std::map<std::string, std::string> loadDbConfig() {
        std::map<std::string, std::string> cfg;
        std::string  confPath = getCanonicalConfigPath();
        std::ifstream f(confPath);
        if (!f.is_open()) {
            std::cerr << "⚠️ [CONFIG] 无法打开配置文件: " << confPath << std::endl;
            return cfg;
        }

        std::string line;
            while (getline(f, line)) {
                auto pos = line.find('=');
                if (pos == std::string::npos) continue;
                auto key = line.substr(0, pos);
                auto val = line.substr(pos+1);
                cfg[key] = val;
            }
            return cfg;
    }


    BusinessServer::BusinessServer(net::EventLoop *loop,
                                   const net::InetAddress &listenAddr,
                                   std::string serverName)
        : mainLoop_(loop), server_(std::make_unique<net::TcpServer>(loop, listenAddr, std::move(serverName)))
    {
        // 注册 Muduo 底层核心事件回调
        server_->setConnectionCallback([this](const net::TcpConnectionPtr &conn){ this->onConnection(conn); });
        server_->setMessageCallback([this](const net::TcpConnectionPtr &conn, net::Buffer *buf, net::Timestamp t){ this->onMessage(conn, buf, t); });

        // 🟢 统一接口调度路由器（彻底修复重名与漏项 Bug）
        router_["LOGIN"] = [this](auto c, auto j){ this->handleLogin(c, j); };
        router_["UPLOAD_SCHEDULE"] = [this](auto c, auto j){ this->handleUploadSchedule(c, j); };
        router_["FILTER_AVAILABLE_MEMBERS"] = [this](auto c, auto j){ this->handleFilterMembers(c, j); };
        router_["CONFIRM_ASSIGN"] = [this](auto c, auto j){ this->handleConfirmAssign(c, j); };
        router_["GET_MEMBERS"] = [this](auto c, auto j){ this->handleGetMembers(c, j); };
        router_["GET_ACTIVITIES"] = [this](auto c, auto j){ this->handleGetActivities(c, j); };
        router_["ADD_ACTIVITY"] = [this](auto c, auto j){ this->handleAddActivity(c, j); };
        router_["GET_DEPARTMENTS"] = [this](auto c, auto j){ this->handleGetDepartments(c, j); };
        router_["BULK_REGISTER_USERS"] = [this](auto c, auto j)        { this->handleBulkRegister(c, j); };
        router_["GET_MANAGEMENT_ACTIVITIES"] = [this](auto c, auto j){ this->handleGetManagementActivities(c, j); };
        router_["DELETE_ACTIVITY"] = [this](auto c, auto j){ this->handleDeleteActivity(c, j); };
        router_["UPDATE_ACTIVITY"] = [this](auto c, auto j){ this->handleUpdateActivity(c, j); };
        router_["GET_ASSIGNED_MEMBERS"] = [this](auto c, auto j){ this->handleGetAssignedMembers(c, j); };
        router_["BATCH_APPLY_CLASS_TEMPLATE"] = [this](auto c, auto j){ this->handleBatchApplyClassTemplate(c, j); };
        router_["GET_TIME_ANALYTICS"] = [this](auto c, auto j){ this->handleGetTimeSlotAnalytics(c, j); };
        router_["GET_CLASS_TEMPLATE"] = [this](auto c, auto j){ this->handleGetClassTemplate(c, j); };
        router_["DELETE_MEMBER"] = [this](auto c, auto j){ this->handleDeleteMember(c, j); };
        router_["APPLY_ACTIVITY"] = [this](auto c, auto j){ this->handleApplyActivity(c, j); };
        router_["LEAVE_ACTIVITY"] = [this](auto c, auto j){ this->handleLeaveActivity(c, j); };
        router_["BATCH_DELETE_MEMBERS"] = [this](auto c, auto j){ this->handleBatchDeleteMembers(c, j); };
        router_["BATCH_ASSIGN_MEMBERS"] = [this](auto c, auto j){ this->handleBatchAssignMembers(c, j); };
        router_["SET_TERM_START"] = [this](auto c, auto j){ this->handleSetTermStart(c, j); };
        router_["GET_TERM_START"] = [this](auto c, auto j){ this->handleGetTermStart(c, j); };
        router_["GET_REGISTERED_CLASSES"] = [this](auto c, auto j){ this->handleGetRegisteredClasses(c, j); };
        router_["APPROVE_APPLICATION"] = [this](auto c, auto j){ this->handleApproveApplication(c, j); };
        router_["SETTLE_ACTIVITY"] = [this](auto c, auto j){ this->handleSettleActivity(c, j); };


        auto cfg = loadDbConfig();

        std::string db_host = cfg["DB_HOST"];
        std::string db_user = cfg["DB_USER"];
        std::string db_pass = cfg["DB_PASS"];
        std::string db_name = cfg["DB_NAME"];
        int         db_port = 3306;

        if (!cfg["DB_PORT"].empty()) {
            db_port = std::stoi(cfg["DB_PORT"]);
        }

        m_mysql = mysql_init(nullptr);
        if (!mysql_real_connect(
                m_mysql,
                db_host.c_str(),
                db_user.c_str(),
                db_pass.c_str(),
                db_name.c_str(),
                db_port,
                nullptr, 0))
        {
            std::cerr << "❌ [MySQL Connection Fatal] " << mysql_error(m_mysql) << std::endl;
        }
        else
        {
            mysql_set_character_set(m_mysql, "utf8mb4");
            std::cout << "🟢 [MySQL Pipeline] Secure channel established." << std::endl;
            loadSystemConfig();
        }
    }

    BusinessServer::~BusinessServer()
    {
        if (m_mysql)
            mysql_close(m_mysql);
    }

    bool BusinessServer::isAuthorized(const json &jsonObj, int minRoleLevel) {
        if (!jsonObj.contains("request_user_role")) return false;
        return jsonObj["request_user_role"].get<int>() <= minRoleLevel;
    }

    std::string BusinessServer::escapeString(const std::string &str)
    {
        if (!m_mysql || str.empty())
            return str;
        
        size_t len = str.length() * 2 + 1;
        std::vector<char> buffer(len);
        mysql_real_escape_string(m_mysql, buffer.data(), str.c_str(), str.length());
        return std::string(buffer.data());
    }


    void BusinessServer::loadSystemConfig()
    {
        if (!m_mysql) return;

        // 1. 尝试查询
        std::string query = "SELECT config_value FROM sys_config WHERE config_key = 'term_start_date'";
        
        if (mysql_query(m_mysql, query.c_str()) == 0)
        {
            MYSQL_RES *result = mysql_store_result(m_mysql);
            if (result)
            {
                MYSQL_ROW row = mysql_fetch_row(result);
                if (row)
                {
                    // 成功读取到现有配置
                    m_cachedTermStartDate = row[0];
                    std::cout << "🟢 [SystemConfig] Term start date loaded: " << m_cachedTermStartDate << std::endl;
                }
                else
                {
                    // 2. 🟢 自愈逻辑：如果表里没记录，自动初始化一个默认日期 (比如今天)
                    std::cout << "⚠️ [SystemConfig] No term_start_date found, initializing with default..." << std::endl;
                    
                    // 获取当前日期作为默认值
                    auto now = std::chrono::system_clock::now();
                    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
                    char buffer[11];
                    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", std::localtime(&now_time));
                    std::string defaultDate(buffer);

                    // 执行初始化插入
                    std::string initSql = "INSERT INTO sys_config (config_key, config_value, updated_at) "
                                        "VALUES ('term_start_date', '" + defaultDate + "', NOW());";
                    
                    if (mysql_query(m_mysql, initSql.c_str()) == 0) {
                        m_cachedTermStartDate = defaultDate;
                        std::cout << "🟢 [SystemConfig] Initialized default date: " << m_cachedTermStartDate << std::endl;
                    } else {
                        std::cerr << "❌ [SystemConfig] Failed to initialize default date: " << mysql_error(m_mysql) << std::endl;
                    }
                }
                mysql_free_result(result);
            }
        }
        else
        {
            std::cerr << "❌ [SystemConfig] Query failed: " << mysql_error(m_mysql) << std::endl;
        }
    }

    int BusinessServer::getCurrentWeek()
    {
        if (m_cachedTermStartDate.empty())
        {
            return 0;
        }
        
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm = *std::localtime(&now_time);
        
        char dateStr[11];
        std::strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &now_tm);
        std::string currentDate(dateStr);
        
        int week = 0, day = 0;
        if (TimeConverter::convertDateToWeekAndDay(m_cachedTermStartDate, currentDate, week, day))
        {
            return week;
        }
        return 0;
    }

    void BusinessServer::rebuildUserSchedules(int userId)
    {
        if (!m_mysql)
            return;

        // 1. 从 course_templates 读取用户的课表模板（包含 time_mask）
        std::string sql = "SELECT day_of_week, time_mask, start_week, end_week, week_type FROM course_templates WHERE user_id = " + std::to_string(userId) + " AND class_identifier IS NULL;";
        
        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            std::cerr << "[Schedule Rebuild] Failed to query course templates for user " << userId << ": " << mysql_error(m_mysql) << std::endl;
            return;
        }

        MYSQL_RES *result = mysql_store_result(m_mysql);
        if (!result)
            return;

        // 2. 准备课表规则
        struct CourseRule
        {
            int dayOfWeek;
            uint32_t timeMask;
            int startWeek;
            int endWeek;
            int weekType;
        };
        std::vector<CourseRule> rules;

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result)))
        {
            CourseRule rule;
            rule.dayOfWeek = std::stoi(row[0]);
            rule.timeMask = row[1] ? std::stoul(row[1]) : 0;
            rule.startWeek = std::stoi(row[2]);
            rule.endWeek = std::stoi(row[3]);
            rule.weekType = row[4] ? std::stoi(row[4]) : 0;
            rules.push_back(rule);
        }
        mysql_free_result(result);

        // 3. 遍历每周，计算并更新位图
        for (int week = 1; week <= 16; ++week)
        {
            // 按天计算位图
            for (int day = 1; day <= 7; ++day)
            {
                uint32_t dayMask = 0;
                
                for (const auto &rule : rules)
                {
                    // 检查是否在当天且周范围包含当前周
                    if (rule.dayOfWeek != day)
                        continue;
                    if (week < rule.startWeek || week > rule.endWeek)
                        continue;
                    
                    // 检查周类型（单双周）
                    if (rule.weekType == 1 && week % 2 == 0) // 单周
                        continue;
                    if (rule.weekType == 2 && week % 2 == 1) // 双周
                        continue;
                    
                    // 使用前端传递的 time_mask
                    dayMask |= rule.timeMask;
                }
                
                // 更新 schedules 表
                std::string updateSql =
                    "INSERT INTO schedules (user_id, week_number, day_of_week, day_bitmask) "
                    "VALUES (" + std::to_string(userId) + ", " +
                    std::to_string(week) + ", " +
                    std::to_string(day) + ", " +
                    std::to_string(dayMask) + ") " +
                    "ON DUPLICATE KEY UPDATE day_bitmask = " + std::to_string(dayMask) + ";";
                
                mysql_query(m_mysql, updateSql.c_str());
            }
        }
        
        std::cout << "[Schedule Rebuild] Successfully rebuilt schedules for user " << userId << std::endl;
    }

    void BusinessServer::setThreadNum(int numThreads) { server_->setThreadNum(numThreads); }
    void BusinessServer::start() { server_->start(); }

    void BusinessServer::onConnection(const net::TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            std::cout << "📡 [Network Core] Dynamic connection established from " << conn->peerAddress().toIpPort() << " (fd=" << conn->fd() << ")" << std::endl;
        }
        else
        {
            std::cout << "🔌 [Network Core] Dynamic connection released gracefully (fd=" << conn->fd() << ")" << std::endl;
        }
    }

    void BusinessServer::onMessage(const net::TcpConnectionPtr &conn, net::Buffer *buffer, net::Timestamp)
    {
        std::string jsonBody;

        // 🟢 修复：使用 tryExtractPacket 循环从缓冲区中提取完整的 TCP 数据包
        while (tryExtractPacket(buffer, jsonBody))
        {
            std::cout << "📥 [Network Engine] Inbound Message Payload: " << jsonBody << std::endl;

            json jsonObj;
            try
            {
                jsonObj = json::parse(jsonBody);
            }
            catch (const json::parse_error &e)
            {
                std::cerr << "❌ [Network Engine] JSON Parse Intercepted: " << e.what() << std::endl;
                sendError(conn, -1, "Invalid JSON data wire structure");
                continue; // 继续处理下一个包，而不是直接 return
            }

            if (jsonObj.contains("action") && jsonObj["action"].is_string())
            {
                dispatchAction(conn, jsonObj);
            }
        }
    }

    bool BusinessServer::tryExtractPacket(net::Buffer *buffer, std::string &outJsonBody)
    {
        size_t readable = buffer->readableBytes();
        if (readable < protocol::kHeaderSize)
            return false;

        uint32_t bodyLenNet = 0;
        std::memcpy(&bodyLenNet, buffer->peek(), protocol::kHeaderSize);
        uint32_t bodyLen = ntohl(bodyLenNet);

        if (bodyLen == 0)
        {
            buffer->retrieve(protocol::kHeaderSize);
            outJsonBody.clear();
            return true;
        }

        if (bodyLen > protocol::kMaxBodyLen)
        {
            std::cerr << "[BusinessServer] Protocol error: bodyLen=" << bodyLen << " exceeds limit. Closing connection." << std::endl;
            buffer->retrieveAll();
            return false;
        }

        if (buffer->readableBytes() < protocol::kHeaderSize + bodyLen)
            return false;

        buffer->retrieve(protocol::kHeaderSize);
        outJsonBody = buffer->retrieveAsString(bodyLen);
        return true;
    }

    void BusinessServer::dispatchAction(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        std::string action = jsonObj["action"].get<std::string>();
        auto it = router_.find(action);
        if (it != router_.end())
        {
            it->second(conn, jsonObj);
        }
        else
        {
            handleUnknownAction(conn, jsonObj);
        }
    }

    void BusinessServer::handleLogin(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        if (!jsonObj.contains("student_id") || !jsonObj["student_id"].is_string() ||
            !jsonObj.contains("password") || !jsonObj["password"].is_string())
        {
            sendError(conn, -101, "Missing or invalid student_id/password");
            return;
        }

        std::string studentId = jsonObj["student_id"].get<std::string>();
        std::string password = jsonObj["password"].get<std::string>();

        if (!m_mysql)
        {
            sendError(conn, -500, "Internal Database Handle Invalid");
            return;
        }

        std::string escapedStudentId = escapeString(studentId);
        std::string sql = "SELECT u.id, u.name, u.password_hash, u.role_id, u.department_id,u.class_name, u.status FROM users u WHERE u.student_id = '" + escapedStudentId + "';";
        std::cout << "[MySQL Pipeline] Authenticating user: " << studentId << std::endl;

        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            sendError(conn, -501, "Database Query Failure");
            return;
        }

        MYSQL_RES *result = mysql_store_result(m_mysql);
        MYSQL_ROW row = result ? mysql_fetch_row(result) : nullptr;
        if (!row)
        {
            if (result)
                mysql_free_result(result);
            sendError(conn, -102, "Student ID does not exist");
            return;
        }

        int dbUserId = std::stoi(row[0]);
        std::string dbName = row[1] ? row[1] : "";
        std::string dbPwd = row[2] ? row[2] : "";
        int dbRoleId = std::stoi(row[3]);
        int dbDeptId = row[4] ? std::stoi(row[4]) : 0;
        std::string clsName = row[5] ? row[5] : "";
        int dbStatus = std::stoi(row[6]);
        mysql_free_result(result);

        if (dbStatus == 0)
        {
            sendError(conn, -103, "This account has been banned");
            return;
        }
        if (password != dbPwd)
        {
            sendError(conn, -104, "Incorrect password");
            return;
        }

        json resp = {{"status", "ok"}, {"code", 0}, {"action", "LOGIN"}};
        resp["data"]["user_id"] = dbUserId;
        resp["data"]["name"] = dbName;
        resp["data"]["student_id"] = studentId;
        resp["data"]["role_id"] = dbRoleId;
        resp["data"]["department_id"] = dbDeptId;
        resp["data"]["class_name"] = clsName;
        resp["data"]["message"] = "Welcome back, " + dbName;

        std::cout << "🟢 [Login Success] User " << dbName << " logged in." << std::endl;
        std::cout << "🟢 [Login Success] Data: " << resp["data"].dump() << std::endl;
        sendResponse(conn, resp);
    }

    void BusinessServer::handleGetRegisteredClasses(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        if (!m_mysql)
        {
            sendError(conn, -500, "Database handle down");
            return;
        }

        std::string sql = "SELECT class_name FROM view_major_class_stats ORDER BY class_name ASC;";

        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            sendError(conn, -501, "Failed to pull registered classes: " + std::string(mysql_error(m_mysql)));
            return;
        }

        MYSQL_RES *res = mysql_store_result(m_mysql);
        json classList = json::array();

        if (res)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)))
            {
                classList.push_back(row[0]); // 将真实班级（如"网络工程2502"）塞进数组
            }
            mysql_free_result(res);
        }

        json resp = {
            {"status", "ok"},
            {"code", 0},
            {"action", "GET_REGISTERED_CLASSES"},
            {"data", {{"classes", classList}}}};
        sendResponse(conn, resp);
    }

    void BusinessServer::handleUploadSchedule(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        if (!jsonObj.contains("user_id") || !jsonObj.contains("courses") || !jsonObj["courses"].is_array())
        {
            sendError(conn, -11, "Param error");
            return;
        }

        int userId = jsonObj["user_id"].get<int>();
        auto courses = jsonObj["courses"];
        bool isPublic = jsonObj.value("is_public", false);
        std::string targetClass = jsonObj.value("target_class", "");

        mysql_query(m_mysql, "START TRANSACTION;");

        try
        {
            if (isPublic)
            {
                // 1. 管理员模式：操作公共空间 (user_id IS NULL)
                std::string delSql = "DELETE FROM course_templates WHERE class_identifier = '" + targetClass + "' AND user_id IS NULL;";
                mysql_query(m_mysql, delSql.c_str());

                for (const auto &c : courses)
                {
                    std::string cName = c.value("course_name", "有课");
                    uint32_t timeMask = c.value("time_mask", 0);
                    // 必须保证插入 user_id 为 NULL
                    std::string ins = "INSERT INTO course_templates (user_id, class_identifier, course_name, day_of_week, period, time_mask, start_week, end_week, week_type) VALUES "
                                      "(NULL, '" +
                                      targetClass + "', '" + cName + "', " +
                                      std::to_string(c["day_of_week"].get<int>()) + ", " +
                                      std::to_string(c["period"].get<int>()) + ", " +
                                      std::to_string(timeMask) + ", " +
                                      std::to_string(c.value("start_week", 1)) + ", " +
                                      std::to_string(c.value("end_week", 16)) + ", " +
                                      std::to_string(c.value("week_type", 0)) + ");";
                    mysql_query(m_mysql, ins.c_str());
                }
            }
            else
            {
                // 2. 个人模式：操作个人空间 (user_id = userId)
                std::string delSql = "DELETE FROM course_templates WHERE user_id = " + std::to_string(userId) + " AND class_identifier IS NULL;";
                mysql_query(m_mysql, delSql.c_str());

                for (const auto &c : courses)
                {
                    std::string cName = c.value("course_name", "有课");
                    uint32_t timeMask = c.value("time_mask", 0);
                    std::string ins = "INSERT INTO course_templates (user_id, class_identifier, course_name, day_of_week, period, time_mask, start_week, end_week, week_type) VALUES (" +
                                      std::to_string(userId) + ", NULL, '" + cName + "', " +
                                      std::to_string(c["day_of_week"].get<int>()) + ", " +
                                      std::to_string(c["period"].get<int>()) + ", " +
                                      std::to_string(timeMask) + ", " +
                                      std::to_string(c.value("start_week", 1)) + ", " +
                                      std::to_string(c.value("end_week", 16)) + ", " +
                                      std::to_string(c.value("week_type", 0)) + ");";
                    mysql_query(m_mysql, ins.c_str());
                }
            }
            mysql_query(m_mysql, "COMMIT;");
            
            // 上传课表后重建用户的 schedule 位图缓存
            rebuildUserSchedules(userId);
            
            sendResponse(conn, {{"status", "ok"}, {"action", "UPLOAD_SCHEDULE"}});
        }
        catch (const std::exception &e)
        {
            mysql_query(m_mysql, "ROLLBACK;");
            sendError(conn, -502, "Transaction failed: " + std::string(e.what()));
        }
    }


    void BusinessServer::handleSettleActivity(const net::TcpConnectionPtr &conn, const json &jsonObj) {
        if (!isAuthorized(jsonObj, ROLE_CAPTAIN)) {
            sendError(conn, -403, "只有队长或老师可以进行结算");
            return;
        }

        int actId = jsonObj["activity_id"].get<int>();
        // 前端传入: [{"user_id": 1, "duration": 2.0, "status": 1}]
        auto results = jsonObj["results"];

        mysql_query(m_mysql, "START TRANSACTION;");

        for (const auto& r : results) {
            std::string sql = "UPDATE activity_members SET duration_hours = " + std::to_string(r["duration"].get<double>()) + 
                            ", is_attended = " + std::to_string(r["status"].get<int>()) + 
                            ", sign_in_status = 1 WHERE activity_id = " + std::to_string(actId) + 
                            " AND user_id = " + std::to_string(r["user_id"].get<int>()) + ";";
            if (mysql_query(m_mysql, sql.c_str()) != 0) {
                mysql_query(m_mysql, "ROLLBACK;"); // 发现错误立即回滚
                sendError(conn, -502, "Failed to update member, transaction rolled back.");
                return;
            }
        }
        // 锁定活动状态为 3 (已结束结算)
        std::string updateActSql = "UPDATE activities SET status = 3 WHERE id = " + std::to_string(actId);
        if (mysql_query(m_mysql, updateActSql.c_str()) != 0) {
            mysql_query(m_mysql, "ROLLBACK;"); 
            sendError(conn, -503, "Failed to update activity status, rolled back.");
            return;
        }
        mysql_query(m_mysql, "COMMIT;");
        sendResponse(conn, {{"status", "ok"}, {"message", "结算完成"}});
    }

    void BusinessServer::handleApplyActivity(const net::TcpConnectionPtr &conn, const json &jsonObj)
{
    // 1. 参数校验
    if (!jsonObj.contains("activity_id") || !jsonObj.contains("user_id")) {
        sendError(conn, -801, "Missing activity_id or user_id");
        return;
    }
    int actId = jsonObj["activity_id"].get<int>();
    int userId = jsonObj["user_id"].get<int>();
    std::string reason = escapeString(jsonObj.value("reason", ""));

    // 2. 状态校验 (确保活动允许报名)
    std::string checkSql = "SELECT status, max_participants, "
                           "(SELECT COUNT(*) FROM activity_members WHERE activity_id = " + std::to_string(actId) + ") as current_count "
                           "FROM activities WHERE id = " + std::to_string(actId) + ";";

    if (mysql_query(m_mysql, checkSql.c_str()) != 0) {
        sendError(conn, -501, "Database pre-check failure");
        return;
    }

    MYSQL_RES *res = mysql_store_result(m_mysql);
    MYSQL_ROW row = res ? mysql_fetch_row(res) : nullptr;
    if (!row) {
        if (res) mysql_free_result(res);
        sendError(conn, -802, "Activity does not exist");
        return;
    }

    int status = std::stoi(row[0]);
    int maxPart = std::stoi(row[1]);
    int currentCount = std::stoi(row[2]);
    mysql_free_result(res);

    if (status != 1) {
        sendError(conn, -803, "报名失败：活动已不再招募阶段");
        return;
    }
    
    // 如果你设置了人数限制，这里可以校验
    if (maxPart > 0 && currentCount >= maxPart) {
        sendError(conn, -804, "报名失败：活动名额已满");
        return;
    }

    // 3. 写入审批表 (而不是直接写入 activity_members)
    // 状态 0 = 待审批
    std::string insertSql = "INSERT INTO activity_applications (activity_id, user_id, apply_reason, status) "
                            "VALUES (" + std::to_string(actId) + ", " + std::to_string(userId) + ", '" + reason + "', 0) "
                            "ON DUPLICATE KEY UPDATE status = 0, apply_reason = '" + reason + "';";

    if (mysql_query(m_mysql, insertSql.c_str()) != 0) {
        sendError(conn, -501, "Failed to submit application: " + std::string(mysql_error(m_mysql)));
        return;
    }

    sendResponse(conn, {{"status", "ok"}, {"action", "APPLY_ACTIVITY"}, {"message", "报名申请已提交，等待审批"}});
}


void BusinessServer::handleApproveApplication(const net::TcpConnectionPtr &conn, const json &jsonObj) {
        // 鉴权：仅老师/队长/部长可操作
        if (!isAuthorized(jsonObj, ROLE_MINISTER)) {
            sendError(conn, -403, "权限不足");
            return;
        }

        int appId = jsonObj["application_id"].get<int>();
        int status = jsonObj["status"].get<int>(); // 1: 通过, 2: 拒绝

        mysql_query(m_mysql, "START TRANSACTION;");
        
        // 更新申请状态
        std::string sql = "UPDATE activity_applications SET status = " + std::to_string(status) + " WHERE id = " + std::to_string(appId) + ";";
        mysql_query(m_mysql, sql.c_str());

        // 如果通过，自动同步到 activity_members 表
        if (status == 1) {
            std::string syncSql = "INSERT INTO activity_members (activity_id, user_id, assign_type) SELECT activity_id, user_id, 2 FROM activity_applications WHERE id = " + std::to_string(appId) + ";";
            mysql_query(m_mysql, syncSql.c_str());
        }

        mysql_query(m_mysql, "COMMIT;");
        sendResponse(conn, {{"status", "ok"}, {"message", "审批已更新"}});
    }


    void BusinessServer::handleGetMembers(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        if (!jsonObj.contains("request_user_role"))
        {
            sendError(conn, -912, "Missing rbac parameters");
            return;
        }
        int roleId = jsonObj["request_user_role"].get<int>();

        // 如果是部长(30)，需要根据部门裁剪数据；老师(10)和队长(20)看全量
        std::string sqlFilter = "";
        if (roleId == 30 && jsonObj.contains("request_user_dept") && !jsonObj["request_user_dept"].is_null())
        {
            int deptId = jsonObj["request_user_dept"].get<int>();
            sqlFilter = " WHERE u.department_id = " + std::to_string(deptId);
        }

        if (!m_mysql)
        {
            sendError(conn, -500, "Database link down");
            return;
        }

        // 核心重构 SQL：
        // 1. 严格使用 u.id (内部唯一主键) 与 activity_members.user_id 进行左连接聚合，严禁使用 student_id 关联
        // 2. 只有当签到状态为已签到(1)或迟到(2)且确认出勤(is_attended=1)时，才计入时长与次数
        std::string sql =
            "SELECT u.id, u.student_id, u.name, u.major, u.class_name, u.role_id, u.status, "
            "d.name AS dept_name, "
            "COALESCE(COUNT(CASE WHEN am.sign_in_status IN (1, 2) AND am.is_attended = 1 THEN am.id END), 0) AS total_count, "
            "COALESCE(SUM(CASE WHEN am.sign_in_status IN (1, 2) AND am.is_attended = 1 THEN am.duration_hours END), 0.0) AS total_hours "
            "FROM users u "
            "LEFT JOIN departments d ON u.department_id = d.id "
            "LEFT JOIN activity_members am ON u.id = am.user_id " +
            sqlFilter + " "
                        "GROUP BY u.id, u.student_id, u.name, u.major, u.class_name, u.role_id, u.status, d.name;";

        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            sendError(conn, -501, "Failed to query members: " + std::string(mysql_error(m_mysql)));
            return;
        }

        MYSQL_RES *res = mysql_store_result(m_mysql);
        json memberList = json::array();

        if (res)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)))
            {
                json m;
                m["user_id"] = row[0] ? std::stoi(row[0]) : 0;
                m["student_id"] = row[1] ? row[1] : "";
                m["name"] = row[2] ? row[2] : "";
                m["major"] = row[3] ? row[3] : "";
                m["class_name"] = row[4] ? row[4] : "";
                m["role_id"] = row[5] ? std::stoi(row[5]) : 40;
                m["status"] = row[6] ? std::stoi(row[6]) : 1;
                m["dept_name"] = row[7] ? row[7] : "未分配";
                m["total_count"] = row[8] ? std::stoi(row[8]) : 0;
                // 处理 decimal 转 float
                m["total_hours"] = row[9] ? std::stof(row[9]) : 0.0f;
                m["current_state"] = "free"; // 运行时动态状态默认为空闲
                memberList.push_back(m);
            }
            mysql_free_result(res);
        }

        json resp = {
            {"status", "ok"},
            {"code", 0},
            {"action", "GET_MEMBERS"},
            {"data", {{"members", memberList}}}};
        sendResponse(conn, resp);
    }

    void BusinessServer::handleBatchDeleteMembers(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        if (!jsonObj.contains("target_user_ids") || !jsonObj["target_user_ids"].is_array())
        {
            sendError(conn, -913, "Missing target_user_ids array parameter");
            return;
        }
        auto targetIds = jsonObj["target_user_ids"];
        if (targetIds.empty())
        {
            sendError(conn, -914, "Target IDs array is empty");
            return;
        }

        if (!m_mysql)
        {
            sendError(conn, -500, "Database link down");
            return;
        }

        // 构建 IN 子句
        std::string idList;
        for (size_t i = 0; i < targetIds.size(); ++i)
        {
            if (i > 0)
                idList += ",";
            idList += std::to_string(targetIds[i].get<int>());
        }

        // 使用 IN 子句一次性删除多个用户
        std::string sql = "DELETE FROM users WHERE id IN (" + idList + ");";

        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            std::cerr << "❌ [Batch Delete Failed] " << mysql_error(m_mysql) << std::endl;
            sendError(conn, -501, "Batch delete failed: " + std::string(mysql_error(m_mysql)));
            return;
        }

        int affectedRows = mysql_affected_rows(m_mysql);

        json resp = {
            {"status", "ok"},
            {"code", 0},
            {"action", "BATCH_DELETE_MEMBERS"},
            {"message", "Batch purge completed successfully."},
            {"data", {{"affected_rows", affectedRows}}}};
        sendResponse(conn, resp);
    }

    void BusinessServer::handleSetTermStart(const net::TcpConnectionPtr &conn, const json &jsonObj) {
        if (!jsonObj.contains("new_date") || !jsonObj["new_date"].is_string())
        {
            sendError(conn, -901, "Missing or invalid new_date parameter");
            return;
        }
        
        std::string newDate = jsonObj["new_date"].get<std::string>();
        std::string escapedDate = escapeString(newDate);
        std::string sql = "INSERT INTO sys_config (config_key, config_value, updated_at) "
                  "VALUES ('term_start_date', '" + escapedDate + "', NOW()) "
                  "ON DUPLICATE KEY UPDATE config_value = VALUES(config_value), updated_at = NOW();";
        
        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            sendError(conn, -501, "Failed to update term start date: " + std::string(mysql_error(m_mysql)));
            return;
        }
        
        m_cachedTermStartDate = newDate;
        
        json resp = {
            {"status", "ok"},
            {"code", 0},
            {"action", "SET_TERM_START"},
            {"message", "学期起始日期已更新"},
            {"data", {
                {"term_start_date", newDate},
                {"current_week", getCurrentWeek()}
            }}
        };
        sendResponse(conn, resp);
    }

    void BusinessServer::handleGetTermStart(const net::TcpConnectionPtr &conn, const json &jsonObj) {
        json resp = {
            {"status", "ok"},
            {"code", 0},
            {"action", "GET_TERM_START"},
            {"data", {
                {"term_start_date", m_cachedTermStartDate},
                {"current_week", getCurrentWeek()}
            }}
        };
        sendResponse(conn, resp);
    }

    void BusinessServer::handleBatchApplyClassTemplate(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        if (!jsonObj.contains("class_name") || !jsonObj["class_name"].is_string())
        {
            sendError(conn, -412, "Missing or invalid class_name parameter");
            return;
        }
        std::string className = jsonObj["class_name"].get<std::string>();
        std::cout << "DEBUG: Fetching template for class: [" << className << "]" << std::endl;

        if (!m_mysql)
        {
            sendError(conn, -500, "Database handle invalid");
            return;
        }

        mysql_query(m_mysql, "START TRANSACTION;");

        // 1. 查询该班级公共模板里的所有课程规则
        std::string sqlTemplate =
            "SELECT day_of_week, period, start_week, end_week, week_type "
            "FROM course_templates WHERE class_identifier = '" +
            className + "' AND user_id IS NULL;";

        if (mysql_query(m_mysql, sqlTemplate.c_str()) != 0)
        {
            mysql_query(m_mysql, "ROLLBACK;");
            sendError(conn, -501, "Failed to fetch class templates: " + std::string(mysql_error(m_mysql)));
            return;
        }

        MYSQL_RES *templateRes = mysql_store_result(m_mysql);
        if (!templateRes)
        {
            mysql_query(m_mysql, "ROLLBACK;");
            sendError(conn, -502, "Store template result failed");
            return;
        }

        // 2. 压缩计算 1-16 周的位图矩阵
        uint32_t weeklyMasks[16] = {0};
        MYSQL_ROW row;

        while ((row = mysql_fetch_row(templateRes)))
        {
            int dayOfWeek = std::stoi(row[0]);
            int period = std::stoi(row[1]);
            int startWeek = std::stoi(row[2]);
            int endWeek = std::stoi(row[3]);
            int weekType = std::stoi(row[4]);

            int bitIndex = (dayOfWeek - 1) * 6 + (period - 1);
            uint32_t courseBit = (1u << bitIndex);

            for (int w = 1; w <= 16; ++w)
            {
                if (w >= startWeek && w <= endWeek)
                {
                    if (weekType == 0 ||
                        (weekType == 1 && w % 2 != 0) ||
                        (weekType == 2 && w % 2 == 0))
                    {
                        weeklyMasks[w - 1] |= courseBit;
                    }
                }
            }
        }
        mysql_free_result(templateRes);

        // ========================================================================
        // 🟢 核心修正点：将 role_id = 40 改为 role_id >= 20
        //    确保本班级的 队长(20)、部长(30)、普通队员(40) 都能一起成功同步到课表位图！
        // ========================================================================
        std::string escapedClassName = escapeString(className);
        std::string sqlUsers = "SELECT id FROM users WHERE class_name = '" + escapedClassName + "' AND role_id >= 20 AND status = 1;";
        if (mysql_query(m_mysql, sqlUsers.c_str()) != 0)
        {
            mysql_query(m_mysql, "ROLLBACK;");
            sendError(conn, -501, "Failed to query class users: " + std::string(mysql_error(m_mysql)));
            return;
        }

        MYSQL_RES *usersRes = mysql_store_result(m_mysql);
        std::vector<int> userIds;
        if (usersRes)
        {
            while ((row = mysql_fetch_row(usersRes)))
            {
                userIds.push_back(std::stoi(row[0]));
            }
            mysql_free_result(usersRes);
        }

        // 4. 批量下发更新
        // 1. 预先将模板存储为结构体，方便后续循环使用
        struct CourseRule
        {
            int dayOfWeek;
            uint32_t mask;
        };
        std::vector<CourseRule> rules;
        // 在 while ((row = mysql_fetch_row(templateRes))) 循环中填充 rules...

        // 2. 修正后的双重循环
        int affectedUsers = 0;
        for (int uid : userIds)
        {
            for (const auto &rule : rules)
            { // 遍历每一条课表规则
                for (int w = 1; w <= 16; ++w)
                { // 遍历 1-16 周
                    // 这里根据 rule.mask 计算当前周的掩码
                    uint32_t finalMask = rule.mask;

                    std::string sqlSchedule =
                        "INSERT INTO schedules (user_id, week_number, day_of_week, day_bitmask) "
                        "VALUES (" +
                        std::to_string(uid) + ", " +
                        std::to_string(w) + ", " +
                        std::to_string(rule.dayOfWeek) + ", " + // 这里的 d 变成了 rule.dayOfWeek
                        std::to_string(finalMask) + ") " +
                        "ON DUPLICATE KEY UPDATE day_bitmask = " + std::to_string(finalMask) + ";";

                    if (mysql_query(m_mysql, sqlSchedule.c_str()) != 0)
                    {
                        mysql_query(m_mysql, "ROLLBACK;");
                        sendError(conn, -503, "SQL Error: " + std::string(mysql_error(m_mysql)));
                        return;
                    }
                }
            }
            affectedUsers++;
        }

        mysql_query(m_mysql, "COMMIT;");

        json resp = {
            {"status", "ok"},
            {"code", 0},
            {"action", "BATCH_APPLY_CLASS_TEMPLATE"},
            {"data", {{"affected_rows", affectedUsers}}} // 保持与前面优化对齐的键名
        };
        sendResponse(conn, resp);
    }

    void BusinessServer::handleGetTimeSlotAnalytics(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        if (!jsonObj.contains("target_week") || !jsonObj.contains("time_mask"))
        {
            sendError(conn, -415, "Missing analytical tracking bounds");
            return;
        }

        int targetWeek = jsonObj["target_week"].get<int>();
        uint32_t checkMask = jsonObj["time_mask"].get<uint32_t>(); // 待检查的 15 位单日小时掩码
        int dayOfWeek = jsonObj.value("day_of_week", 1);           // 默认测算周一
        std::string targetClass = jsonObj.value("target_class", "");

        if (!m_mysql)
        {
            sendError(conn, -500, "Database link closed");
            return;
        }

        std::string sql =
            "SELECT u.id, u.student_id, u.name, u.gender, u.role_id, d.name AS dept_name, u.major, u.class_name, "
            "COALESCE(s.day_bitmask, 0) AS current_schedule_mask, "
            "COUNT(CASE WHEN am.is_attended = 1 THEN 1 END) AS total_count, "
            "COALESCE(SUM(CASE WHEN am.is_attended = 1 THEN am.duration_hours END), 0.0) AS total_hours "
            "FROM users u "
            // 🌟 精准按周、天关联
            "LEFT JOIN schedules s ON u.id = s.user_id AND s.week_number = " +
            std::to_string(targetWeek) + " AND s.day_of_week = " + std::to_string(dayOfWeek) + " "
                                                                                               "LEFT JOIN departments d ON u.department_id = d.id "
                                                                                               "LEFT JOIN activity_members am ON u.id = am.user_id "
                                                                                               "WHERE u.role_id >= 20 AND u.status = 1 ";

        if (!targetClass.empty())
        {
            sql += "AND u.class_name = '" + targetClass + "' ";
        }
        sql += "GROUP BY u.id, s.day_bitmask, d.name ORDER BY u.student_id ASC;";

        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            sendError(conn, -501, "Matrix analysis crash: " + std::string(mysql_error(m_mysql)));
            return;
        }

        MYSQL_RES *result = mysql_store_result(m_mysql);
        json freeMembers = json::array();
        json busyMembers = json::array();
        int totalSquad = 0;
        int freeSquad = 0;

        if (result)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result)))
            {
                totalSquad++;
                uint32_t schedMask = row[8] ? std::stoul(row[8]) : 0u;

                json m;
                m["user_id"] = std::stoi(row[0]);
                m["student_id"] = row[1] ? row[1] : "";
                m["name"] = row[2] ? row[2] : "";
                m["gender"] = row[3] ? std::stoi(row[3]) : 0;
                m["role_id"] = row[4] ? std::stoi(row[4]) : 40;
                m["dept_name"] = row[5] ? row[5] : "未分配";
                m["major"] = row[6] ? row[6] : "";
                m["class_name"] = row[7] ? row[7] : "";
                m["total_count"] = std::stoi(row[9]);
                m["total_hours"] = std::stod(row[10]);

                // 🌟 利用 15 位掩码进行全天相交过滤
                if ((schedMask & checkMask) == 0)
                {
                    freeSquad++;
                    m["current_state"] = "free";
                    freeMembers.push_back(m);
                }
                else
                {
                    m["current_state"] = "busy_course";
                    busyMembers.push_back(m);
                }
            }
            mysql_free_result(result);
        }

        double freeRate = totalSquad > 0 ? (double)freeSquad / totalSquad * 100.0 : 0.0;

        json resp = {
            {"status", "ok"},
            {"action", "GET_TIME_ANALYTICS"},
            {"data", {{"total_count", totalSquad}, {"free_count", freeSquad}, {"free_rate", freeRate}, {"free_members", freeMembers}, {"busy_members", busyMembers}}}};
        sendResponse(conn, resp);
    }
    // ============================================================================
    // Handler：智能排班无课筛选（融合高阶多维统计三维指标 + 专业班级扩容）

    void BusinessServer::handleFilterMembers(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        if (!jsonObj.contains("activity_week") || !jsonObj.contains("time_mask"))
        {
            sendError(conn, -305, "Missing scheduling bits for calendar dynamic pipeline");
            return;
        }

        int week = jsonObj["activity_week"].get<int>();
        uint32_t mask = jsonObj["time_mask"].get<uint32_t>(); // 当前活动的 15 位单日小时掩码

        // 🌟 为了知道活动是在星期几，前端需要在 jsonObj 中把活动的绝对日期或者计算好的 day_of_week 传回来
        // 我们在这里做防御性兼容：默认前端传入 day_of_week，若没有则默认取周六(6)或周日(7)进行测算
        int dayOfWeek = jsonObj.value("day_of_week", 6);

        if (!m_mysql)
        {
            sendError(conn, -500, "Database link closed");
            return;
        }

        // 🎯 精准位元穿透重写 SQL 核心
        std::string sql =
            "SELECT u.id, u.name, u.student_id, d.name AS dept_name, u.role_id, u.gender, "
            "u.phone, u.major, u.class_name, "
            "COUNT(CASE WHEN am.is_attended = 1 THEN 1 END) AS total_count, "
            "COALESCE(SUM(CASE WHEN am.is_attended = 1 THEN am.duration_hours END), 0.0) AS total_hours, "
            "COALESCE(MAX(CASE WHEN am.is_attended = 1 THEN am.updated_at END), '1970-01-01 00:00:00') AS last_time, "

            // 动态检测其当前是否正在参与其他活动
            "(SELECT COUNT(*) FROM activity_members active_am "
            " INNER JOIN activities active_act ON active_am.activity_id = active_act.id "
            " WHERE active_am.user_id = u.id AND active_act.status = 2) AS is_in_activity, "

            "COALESCE(s.day_bitmask, 0) AS current_schedule_mask "
            "FROM users u "

            // 🌟 改动点：精准关联到该周、该星期的单日掩码行
            "LEFT JOIN schedules s ON u.id = s.user_id AND s.week_number = " +
            std::to_string(week) + " AND s.day_of_week = " + std::to_string(dayOfWeek) + " "
                                                                                         "LEFT JOIN departments d ON u.department_id = d.id "
                                                                                         "LEFT JOIN activity_members am ON u.id = am.user_id "

                                                                                         "WHERE u.role_id >= 20 AND u.status = 1 "

                                                                                         // 🌟 拦截课表冲突：当天单日 15 位小时忙碌掩码与活动掩码无任何交集
                                                                                         "AND (COALESCE(s.day_bitmask, 0) & " +
            std::to_string(mask) + ") = 0 "

                                   // 🌟 拦截活动冲突：在该周、该星期的同一天内，没有其他进行中且时间网格重叠的活动
                                   "AND NOT EXISTS ("
                                   "    SELECT 1 FROM activity_members busy_am "
                                   "    INNER JOIN activities busy_act ON busy_am.activity_id = busy_act.id "
                                   "    WHERE busy_am.user_id = u.id "
                                   "    AND busy_act.activity_week = " +
            std::to_string(week) + " "
                                   "    AND busy_act.id IN (SELECT id FROM activities WHERE DATE(start_date) = (SELECT DATE(start_date) FROM activities WHERE id = busy_am.activity_id)) " // 判定为同一绝对天
                                   "    AND (busy_act.time_mask & " +
            std::to_string(mask) + ") != 0 "
                                   "    AND busy_act.status IN (1, 2)"
                                   ") "
                                   "GROUP BY u.id, s.day_bitmask, d.name ORDER BY u.student_id ASC;";

        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            sendError(conn, -501, "Calendar matrix filter query failure: " + std::string(mysql_error(m_mysql)));
            return;
        }

        MYSQL_RES *result = mysql_store_result(m_mysql);
        json memberList = json::array();

        if (result)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result)))
            {
                json m;
                m["user_id"] = std::stoi(row[0]);
                m["name"] = row[1] ? row[1] : "";
                m["student_id"] = row[2] ? row[2] : "";
                m["dept_name"] = row[3] ? row[3] : "未分配";
                m["role_id"] = row[4] ? std::stoi(row[4]) : 40;
                m["gender"] = row[5] ? std::stoi(row[5]) : 0;
                m["phone"] = row[6] ? row[6] : "-";
                m["major"] = row[7] ? row[7] : "";
                m["class_name"] = row[8] ? row[8] : "";
                m["total_count"] = std::stoi(row[9]);
                m["total_hours"] = std::stod(row[10]);
                m["last_time"] = row[11] ? row[11] : "无记录";

                int isInActivity = std::stoi(row[12]);
                uint32_t schedMask = row[13] ? std::stoul(row[13]) : 0u;

                if (isInActivity > 0)
                    m["current_state"] = "busy_activity";
                else if ((schedMask & mask) != 0u)
                    m["current_state"] = "busy_course";
                else
                    m["current_state"] = "free";

                memberList.push_back(m);
            }
            mysql_free_result(result);
        }

        json resp = {{"status", "ok"}, {"action", "FILTER_AVAILABLE_MEMBERS"}, {"data", {{"members", memberList}}}};
        sendResponse(conn, resp);
    }

    void BusinessServer::handleBatchAssignMembers(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        if (!jsonObj.contains("activity_id") || !jsonObj.contains("user_ids") || !jsonObj["user_ids"].is_array())
        {
            sendError(conn, -380, "Missing parameters for batch assignment");
            return;
        }

        int activityId = jsonObj["activity_id"].get<int>();
        auto userIds = jsonObj["user_ids"];

        if (userIds.empty())
        {
            sendError(conn, -381, "Purged user IDs array is empty");
            return;
        }

        if (!m_mysql)
        {
            sendError(conn, -500, "Database link closed");
            return;
        }

        // 开启显式事务，保证所有人一气呵成写进去，只要有一个由于极端原因失败，整体回滚
        mysql_query(m_mysql, "START TRANSACTION;");

        int assignedCount = 0;
        for (const auto &idJson : userIds)
        {
            int userId = idJson.get<int>();

            // 插入到活动录用排班表，默认指派类型为 1=智能排班，签到状态 0=未签到
            std::string sql =
                "INSERT INTO activity_members (activity_id, user_id, assign_type, sign_in_status, duration_hours, is_attended) "
                "VALUES (" +
                std::to_string(activityId) + ", " + std::to_string(userId) + ", 1, 0, 0.0, 0) "
                                                                             "ON DUPLICATE KEY UPDATE assign_type = 1;"; // 如果已经存在，则更新为排班状态

            if (mysql_query(m_mysql, sql.c_str()) == 0)
            {
                assignedCount++;
            }
            else
            {
                std::cerr << "❌ [Batch Assign Failed for User " << userId << "] " << mysql_error(m_mysql) << std::endl;
                mysql_query(m_mysql, "ROLLBACK;");
                sendError(conn, -501, "Transaction rollback due to: " + std::string(mysql_error(m_mysql)));
                return;
            }
        }

        // 整个数组顺利插入，提交事务
        mysql_query(m_mysql, "COMMIT;");

        json resp = {
            {"status", "ok"},
            {"code", 0},
            {"action", "BATCH_ASSIGN_MEMBERS"},
            {"message", "Successfully assigned " + std::to_string(assignedCount) + " members to activity."},
            {"data", {{"assigned_count", assignedCount}}}};
        sendResponse(conn, resp);
    }

    void BusinessServer::handleConfirmAssign(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        if (!jsonObj.contains("activity_id") || !jsonObj["user_ids"].is_array())
        {
            sendError(conn, -301, "Missing or invalid activity_id/user_ids");
            return;
        }

        int activityId = jsonObj["activity_id"].get<int>();
        auto userIds = jsonObj["user_ids"];
        if (userIds.empty())
        {
            sendError(conn, -302, "User IDs array cannot be empty");
            return;
        }

        std::string sql = "INSERT INTO activity_members (activity_id, user_id, assign_type, sign_in_status) VALUES ";
        for (size_t i = 0; i < userIds.size(); ++i)
        {
            sql += "(" + std::to_string(activityId) + ", " + std::to_string(userIds[i].get<int>()) + ", 1, 0)";
            if (i < userIds.size() - 1)
                sql += ", ";
        }
        sql += " ON DUPLICATE KEY UPDATE updated_at = NOW();";

        std::cout << "[MySQL Pipeline] Bulk Assigning Members: " << sql << std::endl;
        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            sendError(conn, -501, "Database Bulk Insert Failure");
            return;
        }

        json resp = {{"status", "ok"}, {"code", 0}, {"action", "CONFIRM_ASSIGN"}};
        resp["message"] = "Successfully deployed " + std::to_string(userIds.size()) + " members.";
        sendResponse(conn, resp);
    }

    void BusinessServer::handleBulkRegister(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        if (!jsonObj.contains("users") || !jsonObj["users"].is_array() || jsonObj["users"].empty())
        {
            sendError(conn, -701, "Invalid mass registry block payload");
            return;
        }

        auto users = jsonObj["users"];
        // 🟢 扩容支持 major 和 class_name
        std::string sql = "INSERT INTO users (name, student_id, password_hash, role_id, department_id, major, class_name, status) VALUES ";
        for (size_t i = 0; i < users.size(); ++i)
        {
            auto u = users[i];
            std::string name = escapeString(u["name"].get<std::string>());
            std::string studentId = escapeString(u["student_id"].get<std::string>());
            std::string password = escapeString(u["password"].get<std::string>());
            std::string major = escapeString(u.value("major", ""));
            std::string className = escapeString(u.value("class_name", ""));

            sql += "('" + name + "', '" + studentId + "', '" + password + "', " + std::to_string(u["role_id"].get<int>()) + ", " + std::to_string(u["department_id"].get<int>()) + ", '" + major + "', '" + className + "', 1)";
            if (i < users.size() - 1)
                sql += ", ";
        }
        sql += " ON DUPLICATE KEY UPDATE name=VALUES(name), role_id=VALUES(role_id), major=VALUES(major), class_name=VALUES(class_name);";

        std::cout << "[MySQL Bulk Register Pipeline] Processing..." << std::endl;
        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            sendError(conn, -501, mysql_error(m_mysql));
            return;
        }

        json resp = {{"status", "ok"}, {"action", "BULK_REGISTER_USERS"}};
        resp["data"]["count"] = users.size();
        sendResponse(conn, resp);
    }

    void BusinessServer::handleGetDepartments(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        if (!m_mysql)
        {
            sendError(conn, -500, "Database down");
            return;
        }

        std::string sql = "SELECT id, name FROM departments ORDER BY sort_order ASC;";
        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            sendError(conn, -501, "Failed to query departments: " + std::string(mysql_error(m_mysql)));
            return;
        }

        MYSQL_RES *result = mysql_store_result(m_mysql);
        if (!result)
        {
            sendError(conn, -501, "Failed to get departments result");
            return;
        }

        json departments = json::array();
        
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result)))
        {
            int did = std::stoi(row[0]);
            std::string name = row[1] ? row[1] : "";
            departments.push_back({{"text", name}, {"did", did}});
        }
        mysql_free_result(result);

        sendResponse(conn, {
            {"status", "ok"},
            {"code", 0},
            {"action", "GET_DEPARTMENTS"},
            {"data", {{"departments", departments}}}
        });
    }

    void BusinessServer::handleAddActivity(const net::TcpConnectionPtr &conn, const json &jsonObj)
{
    // 1. 基础参数校验（如果前端传了 instances，优先用 instances；否则兼容单次模式）
    if (!jsonObj.contains("instances") || !jsonObj["instances"].is_array()) {
        sendError(conn, -401, "Missing activity instances array");
        return;
    }

    auto instances = jsonObj["instances"];
    std::string title = escapeString(jsonObj.value("title", "活动"));
    std::string desc = escapeString(jsonObj.value("description", "无描述"));
    std::string loc = escapeString(jsonObj.value("location", "无地点"));
    int organizerId = jsonObj.value("organizer_id", 0);
    int deptId = jsonObj.value("department_id", 1);
    int maxParticipants = jsonObj.value("max_participants", 30);
    std::string signDeadline = jsonObj.value("sign_deadline", "");
    int periodType = jsonObj.value("period_type", 0);

    // 2. 开启事务：保证所有重复活动要么全部创建，要么全部失败
    mysql_query(m_mysql, "START TRANSACTION;");

    for (const auto &item : instances) {
        std::string startDateStr = item["date"].get<std::string>();
        std::string startTimeStr = item["start_time"].get<std::string>();
        std::string endTimeStr = item["end_time"].get<std::string>();

        int activityWeek = 1;
        int dayOfWeek = 1;
        TimeConverter::convertDateToWeekAndDay(m_cachedTermStartDate, startDateStr, activityWeek, dayOfWeek);

        uint32_t hourMask = TimeConverter::calculateHourMask(startTimeStr, endTimeStr);
        double duration = item.value("duration_h", 1.0);

        char durBuf[16];
        snprintf(durBuf, sizeof(durBuf), "%.1f", duration);

        // 3. 执行单条插入
        std::string sqlInsert =
            "INSERT INTO activities (title, description, location, organizer_id, department_id, "
            "activity_week, time_mask, max_participants, sign_deadline, status, default_duration, "
            "start_date, end_date, start_time, end_time, period_type) VALUES ("
            "'" + title + "', '" + desc + "', '" + loc + "', " + std::to_string(organizerId) + ", " + std::to_string(deptId) + ", " +
            std::to_string(activityWeek) + ", " + std::to_string(hourMask) + ", " + std::to_string(maxParticipants) + ", " +
            (signDeadline.empty() ? "NULL" : "'" + escapeString(signDeadline) + "'") + ", 1, " + std::string(durBuf) + ", " +
            "'" + startDateStr + "', '" + startDateStr + "', '" + startTimeStr + "', '" + endTimeStr + "', " + std::to_string(periodType) + ");";

        if (mysql_query(m_mysql, sqlInsert.c_str()) != 0) {
            mysql_query(m_mysql, "ROLLBACK;");
            sendError(conn, -503, "Database save failure: " + std::string(mysql_error(m_mysql)));
            return;
        }
    }
    // 4. 提交所有变更
    mysql_query(m_mysql, "COMMIT;");

    sendResponse(conn, {{"status", "ok"}, {"action", "ADD_ACTIVITY"}, {"message", "批量活动已生成"}});
}

    void BusinessServer::handleGetActivities(const net::TcpConnectionPtr &conn, const json &)
    {
        if (!m_mysql)
        {
            sendError(conn, -500, "Database Invalid");
            return;
        }
        std::string sql = "SELECT id, title, description, location, organizer_id, department_id, activity_week, time_mask, max_participants, sign_deadline, status, default_duration, start_date, end_date, start_time, end_time, period_type FROM activities WHERE status = 1;";
        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            sendError(conn, -501, "Query Failure");
            return;
        }

        MYSQL_RES *result = mysql_store_result(m_mysql);
        json activityList = json::array();
        MYSQL_ROW row;
        while (result && (row = mysql_fetch_row(result)))
        {
            json act;
            act["activity_id"] = std::stoi(row[0]);
            act["title"] = row[1] ? row[1] : "";
            act["description"] = row[2] ? row[2] : "";
            act["location"] = row[3] ? row[3] : "";
            act["organizer_id"] = row[4] ? std::stoi(row[4]) : 0;
            act["department_id"] = row[5] ? std::stoi(row[5]) : 0;
            act["activity_week"] = std::stoi(row[6]);
            act["time_mask"] = std::stoul(row[7]);
            act["max_participants"] = row[8] ? std::stoi(row[8]) : 0;
            act["sign_deadline"] = row[9] ? row[9] : "";
            act["status"] = row[10] ? std::stoi(row[10]) : 0;
            act["default_duration"] = row[11] ? std::stod(row[11]) : 0.0;
            act["start_date"] = row[12] ? row[12] : "";
            act["end_date"] = row[13] ? row[13] : "";
            act["start_time"] = row[14] ? row[14] : "";
            act["end_time"] = row[15] ? row[15] : "";
            act["period_type"] = row[16] ? std::stoi(row[16]) : 0;
            activityList.push_back(act);
        }
        if (result)
            mysql_free_result(result);

        json resp = {{"status", "ok"}, {"code", 0}, {"action", "GET_ACTIVITIES"}};
        resp["data"]["activities"] = activityList;
        sendResponse(conn, resp);
    }

    void BusinessServer::handleGetManagementActivities(const net::TcpConnectionPtr &conn, const json &)
    {
        std::string sql = "SELECT a.id, a.title, a.description, a.location, a.activity_week, a.time_mask, a.max_participants, a.status, a.sign_deadline, a.default_duration, a.start_date, a.end_date, a.start_time, a.end_time, a.period_type, COUNT(am.user_id) AS assigned_count FROM activities a LEFT JOIN activity_members am ON a.id = am.activity_id GROUP BY a.id ORDER BY a.id DESC;";
        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            sendError(conn, -501, "Query Failure");
            return;
        }

        MYSQL_RES *result = mysql_store_result(m_mysql);
        json activityList = json::array();
        MYSQL_ROW row;
        while (result && (row = mysql_fetch_row(result)))
        {
            json act;
            act["activity_id"] = std::stoi(row[0]);
            act["title"] = row[1] ? row[1] : "";
            act["description"] = row[2] ? row[2] : "";
            act["location"] = row[3] ? row[3] : "";
            act["activity_week"] = std::stoi(row[4]);
            act["time_mask"] = std::stoul(row[5]);
            act["max_participants"] = std::stoi(row[6]);
            act["status"] = std::stoi(row[7]);
            act["sign_deadline"] = row[8] ? row[8] : "";
            act["default_duration"] = row[9] ? std::stod(row[9]) : 0.0;
            act["start_date"] = row[10] ? row[10] : "";
            act["end_date"] = row[11] ? row[11] : "";
            act["start_time"] = row[12] ? row[12] : "";
            act["end_time"] = row[13] ? row[13] : "";
            act["period_type"] = row[14] ? std::stoi(row[14]) : 0;
            act["assigned_count"] = std::stoi(row[15]);
            activityList.push_back(act);
        }
        if (result)
            mysql_free_result(result);

        json resp = {{"status", "ok"}, {"code", 0}, {"action", "GET_MANAGEMENT_ACTIVITIES"}};
        resp["data"]["activities"] = activityList;
        sendResponse(conn, resp);
    }

    void BusinessServer::handleGetClassTemplate(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        // 1. 参数校验，必须确保传入 user_id 以便区分个人与班级模板
        if (!jsonObj.contains("class_name") || !jsonObj["class_name"].is_string() || !jsonObj.contains("user_id"))
        {
            sendError(conn, -411, "Missing class_name or user_id");
            return;
        }

        std::string className = jsonObj["class_name"].get<std::string>();
        int userId = jsonObj["user_id"].get<int>();
        bool isPublicMode = jsonObj.value("is_public", false);
        if (!m_mysql)
        {
            sendError(conn, -500, "Database handle invalid");
            return;
        }
        std::string sql;
        if (isPublicMode)
        {
            // 模式 A：管理员编辑公共课表 —— 只查公共部分 (user_id IS NULL)
            sql = "SELECT course_name, day_of_week, period, start_week, end_week, week_type "
                  "FROM course_templates "
                  "WHERE class_identifier = '" +
                  className + "' AND user_id IS NULL;";
        }
        else
        {
            // 模式 B：队员/个人查看 —— 个人优先，公共兜底 (你的原逻辑)
            sql = "SELECT course_name, day_of_week, period, start_week, end_week, week_type "
                  "FROM course_templates "
                  "WHERE (user_id = " +
                  std::to_string(userId) + ") "
                                           "OR (class_identifier = '" +
                  className + "' AND user_id IS NULL) "
                              "ORDER BY user_id DESC;";
        }

        std::cout << "🔍 [Class Template] Fetching for User " << userId << " in " << className << std::endl;

        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            sendError(conn, -501, "Database query failure: " + std::string(mysql_error(m_mysql)));
            return;
        }

        MYSQL_RES *result = mysql_store_result(m_mysql);
        MYSQL_ROW row;
        json coursesArray = json::array();

        // 3. 内存去重：如果在同一时间段，既有个人模板又有公共模板，只保留个人的
        std::set<std::string> seen;
        while (result && (row = mysql_fetch_row(result)))
        {
            // 唯一的 key 是“星期+节次”，确保同一个格子不重复
            std::string key = std::string(row[1]) + "-" + std::string(row[2]);
            if (seen.find(key) == seen.end())
            {
                json c;
                c["course_name"] = row[0] ? row[0] : "有课";
                c["day_of_week"] = std::stoi(row[1]);
                c["period"] = std::stoi(row[2]);
                c["start_week"] = std::stoi(row[3]);
                c["end_week"] = std::stoi(row[4]);
                c["week_type"] = std::stoi(row[5]);
                coursesArray.push_back(c);
                seen.insert(key);
            }
        }
        if (result)
            mysql_free_result(result);

        json resp = {{"status", "ok"}, {"action", "GET_CLASS_TEMPLATE"}};
        resp["data"]["courses"] = coursesArray;
        sendResponse(conn, resp);
    }

    void BusinessServer::handleDeleteActivity(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        int actId = jsonObj["activity_id"].get<int>();
        std::string sql = "DELETE FROM activities WHERE id = " + std::to_string(actId) + ";";
        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            sendError(conn, -501, "Delete Failure");
            return;
        }
        json resp = {{"status", "ok"}, {"action", "DELETE_ACTIVITY"}};
        sendResponse(conn, resp);
    }

    void BusinessServer::handleUpdateActivity(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        int actId = jsonObj["activity_id"].get<int>();
        std::string title = escapeString(jsonObj.value("title", ""));
        std::string desc = escapeString(jsonObj.value("description", ""));
        std::string loc = escapeString(jsonObj.value("location", ""));
        int maxPart = jsonObj.value("max_participants", 30);
        std::string startDate = jsonObj.value("start_date", "");
        std::string endDate = jsonObj.value("end_date", "");
        std::string startTime = jsonObj.value("start_time", "");
        std::string endTime = jsonObj.value("end_time", "");
        std::string signDeadline = jsonObj.value("sign_deadline", "");

        std::vector<std::string> setClauses;
        if (!title.empty()) setClauses.push_back("title = '" + title + "'");
        if (!desc.empty()) setClauses.push_back("description = '" + desc + "'");
        if (!loc.empty()) setClauses.push_back("location = '" + loc + "'");
        if (jsonObj.contains("max_participants")) setClauses.push_back("max_participants = " + std::to_string(maxPart));
        if (!startDate.empty()) setClauses.push_back("start_date = '" + startDate + "'");
        if (!endDate.empty()) setClauses.push_back("end_date = '" + endDate + "'");
        if (!startTime.empty()) setClauses.push_back("start_time = '" + startTime + "'");
        if (!endTime.empty()) setClauses.push_back("end_time = '" + endTime + "'");
        if (!signDeadline.empty()) setClauses.push_back("sign_deadline = '" + signDeadline + "'");

        if (setClauses.empty()) {
            sendError(conn, -401, "No fields to update");
            return;
        }

        std::string sql = "UPDATE activities SET ";
        for (size_t i = 0; i < setClauses.size(); ++i) {
            sql += setClauses[i];
            if (i < setClauses.size() - 1) sql += ", ";
        }
        sql += " WHERE id = " + std::to_string(actId) + ";";

        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            sendError(conn, -501, "Update Failure");
            return;
        }
        json resp = {{"status", "ok"}, {"action", "UPDATE_ACTIVITY"}};
        sendResponse(conn, resp);
    }

    // ============================================================================
    // Handler：普通成员请假 / 取消申请（仅限活动未开始阶段）
    // ============================================================================
    void BusinessServer::handleLeaveActivity(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        if (!jsonObj.contains("activity_id") || !jsonObj.contains("user_id"))
        {
            sendError(conn, -811, "Missing activity_id or user_id");
            return;
        }
        int activityId = jsonObj["activity_id"].get<int>();
        int userId = jsonObj["user_id"].get<int>();

        // 1. 拦截检查活动状态
        // 1. 拦截检查活动状态
        std::string checkSql = "SELECT status FROM activities WHERE id = " + std::to_string(activityId) + ";";

        if (mysql_query(m_mysql, checkSql.c_str()) != 0)
        {
            sendError(conn, -501, "Query active status failure");
            return;
        }

        // 🟢 修复：在这里显式声明并获取结果集指针
        MYSQL_RES *res = mysql_store_result(m_mysql);
        if (!res)
        {
            sendError(conn, -501, "Query active status field lock failure");
            return;
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        int status = row ? std::stoi(row[0]) : 0;
        mysql_free_result(res);

        if (status != 1)
        {
            sendError(conn, -812, "Leave request denied: Cannot cancel assignment after activity starts");
            return;
        }

        // 2. 从关联表中移除该用户
        std::string deleteSql = "DELETE FROM activity_members WHERE activity_id = " + std::to_string(activityId) + " AND user_id = " + std::to_string(userId) + ";";
        if (mysql_query(m_mysql, deleteSql.c_str()) != 0)
        {
            sendError(conn, -501, "Database deletion breakdown");
            return;
        }

        json resp = {{"status", "ok"}, {"code", 0}, {"action", "LEAVE_ACTIVITY"}, {"message", "Successfully left the activity"}};
        sendResponse(conn, resp);
    }

    // ============================================================================
    // Handler：拉取指定活动已录用的成员列表（用于管理端完结工时结算）
    // ============================================================================
    void BusinessServer::handleGetAssignedMembers(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        if (!jsonObj.contains("activity_id"))
        {
            sendError(conn, -901, "Missing activity_id");
            return;
        }
        int activityId = jsonObj["activity_id"].get<int>();

        if (!m_mysql)
        {
            sendError(conn, -500, "Database Invalid");
            return;
        }

        // 联合查询出当前活动关联的所有成员基本信息
        std::string sql =
            "SELECT u.id, u.name, u.student_id, u.major, u.class_name, am.duration_hours, am.is_attended "
            "FROM activity_members am "
            "INNER JOIN users u ON am.user_id = u.id "
            "WHERE am.activity_id = " +
            std::to_string(activityId) + ";";

        std::cout << "[MySQL Pipeline] Fetching assigned members for activity: " << activityId << std::endl;

        if (mysql_query(m_mysql, sql.c_str()) != 0)
        {
            sendError(conn, -501, "Database query failure for assigned members");
            return;
        }

        MYSQL_RES *result = mysql_store_result(m_mysql);
        json memberList = json::array();
        MYSQL_ROW row;

        while (result && (row = mysql_fetch_row(result)))
        {
            json m;
            m["user_id"] = std::stoi(row[0]);
            m["name"] = row[1] ? row[1] : "";
            m["student_id"] = row[2] ? row[2] : "";
            m["major"] = row[3] ? row[3] : "";
            m["class_name"] = row[4] ? row[4] : "";
            m["duration_hours"] = row[5] ? std::stod(row[5]) : 0.0;
            m["is_attended"] = row[6] ? std::stoi(row[6]) : 1; // 默认出勤
            memberList.push_back(m);
        }
        if (result)
            mysql_free_result(result);

        json resp = {{"status", "ok"}, {"code", 0}, {"action", "GET_ASSIGNED_MEMBERS"}};
        resp["data"]["members"] = memberList;
        sendResponse(conn, resp);
    }

    // ============================================================================
    // Handler：管理员单条/批量除名指定成员（联动清理无课表与活动关联）
    // ============================================================================
    void BusinessServer::handleDeleteMember(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        if (!jsonObj.contains("target_user_id"))
        {
            sendError(conn, -911, "Missing target_user_id parameter");
            return;
        }
        int targetId = jsonObj["target_user_id"].get<int>();

        if (!m_mysql)
        {
            sendError(conn, -500, "Database link down");
            return;
        }

        // 开启显式事务，确保级联干净，防止触发器踩脚
        mysql_query(m_mysql, "START TRANSACTION;");

        // 1. 手动清理由于其他复杂关联（如学号关联）可能残存的外围业务表（防幽灵数据兜底）
        // 如果你有其他按 student_id 统计的表，在这里补上 DELETE 语句

        // 2. 斩断主表记录
        std::string sqlUser = "DELETE FROM users WHERE id = " + std::to_string(targetId) + ";";

        std::cout << "🗑️ [Purge Member] Executing delete for user_id=" << targetId << std::endl;

        if (mysql_query(m_mysql, sqlUser.c_str()) != 0)
        {
            std::cerr << "❌ [MySQL Error] " << mysql_error(m_mysql) << std::endl;
            mysql_query(m_mysql, "ROLLBACK;"); // 失败回滚
            sendError(conn, -501, "Database execution break: " + std::string(mysql_error(m_mysql)));
            return;
        }

        // 提交事务
        mysql_query(m_mysql, "COMMIT;");

        // 返回成功响应
        json resp = {{"status", "ok"}, {"code", 0}, {"action", "DELETE_MEMBER"}, {"message", "Member and all related records purged successfully."}};
        sendResponse(conn, resp);
    }

    void BusinessServer::handleUnknownAction(const net::TcpConnectionPtr &conn, const json &jsonObj)
    {
        std::string action = jsonObj.value("action", "(missing)");
        sendError(conn, -99, "Unknown system action route: '" + action + "'");
    }

    void BusinessServer::sendResponse(const net::TcpConnectionPtr &conn, const json &responseJson)
    {
        std::string body = responseJson.dump();
        conn->send(body.c_str(), static_cast<int>(body.size()));
    }

    void BusinessServer::sendError(const net::TcpConnectionPtr &conn, int errorCode, const std::string &errorMessage)
    {
        json resp = {{"status", "error"}, {"code", errorCode}, {"message", errorMessage}};
        sendResponse(conn, resp);
    }

    bool BusinessServer::validateField(const json &jsonObj, const std::string &fieldName, json::value_t expectedType)
    {
        auto it = jsonObj.find(fieldName);
        if (it == jsonObj.end())
            return false;
        return it->type() == expectedType;
    }

} // namespace qingyun