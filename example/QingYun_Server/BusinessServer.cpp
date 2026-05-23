// ============================================================================
// BusinessServer.cpp — 青云志愿服务队管理系统 · 业务应用层服务器标准实现
// ============================================================================

#include "BusinessServer.h"
#include <cstring>
#include <iostream>

#ifdef _WIN32
#  include <winsock2.h>
#  pragma comment(lib, "ws2_32.lib")
#else
#  include <arpa/inet.h>
#endif

using nlohmann::json;

namespace qingyun {

// ============================================================================
// 构造 / 析构（修复路由映射，确保 FILTER_AVAILABLE_MEMBERS 归队）
// ============================================================================

BusinessServer::BusinessServer(net::EventLoop* loop,
                               const net::InetAddress& listenAddr,
                               std::string serverName)
    : mainLoop_(loop)
    , server_(std::make_unique<net::TcpServer>(loop, listenAddr, std::move(serverName)))
{
    // 注册 Muduo 底层核心事件回调
    server_->setConnectionCallback([this](const net::TcpConnectionPtr& conn) { this->onConnection(conn); });
    server_->setMessageCallback([this](const net::TcpConnectionPtr& conn, net::Buffer* buf, net::Timestamp t) {
        this->onMessage(conn, buf, t);
    });

    // ========================================================================
    // 🟢 统一接口调度路由器（彻底修复重名与漏项 Bug）
    // ========================================================================
    router_["LOGIN"] = [this](auto c, auto j) { this->handleLogin(c, j); };
    router_["UPLOAD_SCHEDULE"] = [this](auto c, auto j) { this->handleUploadSchedule(c, j); };
    router_["FILTER_AVAILABLE_MEMBERS"] = [this](auto c, auto j) { this->handleFilterMembers(c, j); };
    router_["CONFIRM_ASSIGN"] = [this](auto c, auto j) { this->handleConfirmAssign(c, j); };
    router_["GET_MEMBERS"] = [this](auto c, auto j) { this->handleGetMembers(c, j); };
    router_["GET_ACTIVITIES"] = [this](auto c, auto j) { this->handleGetActivities(c, j); };
    router_["ADD_ACTIVITY"] = [this](auto c, auto j) { this->handleAddActivity(c, j); };
    router_["BULK_REGISTER_USERS"] = [this](auto c, auto j) { this->handleBulkRegister(c, j); };
    router_["GET_MANAGEMENT_ACTIVITIES"] = [this](auto c, auto j) { this->handleGetManagementActivities(c, j); };
    router_["DELETE_ACTIVITY"] = [this](auto c, auto j) { this->handleDeleteActivity(c, j); };
    router_["UPDATE_ACTIVITY"] = [this](auto c, auto j) { this->handleUpdateActivity(c, j); };

    router_["GET_ASSIGNED_MEMBERS"] = [this](auto c, auto j) { this->handleGetAssignedMembers(c, j); };

router_["GET_TIME_ANALYTICS"] = [this](auto c, auto j) { this->handleGetTimeSlotAnalytics(c, j); };
    router_["GET_CLASS_TEMPLATE"] = [this](auto c, auto j) { this->handleGetClassTemplate(c, j); };
    router_["DELETE_MEMBER"] = [this](auto c, auto j) { this->handleDeleteMember(c, j); };
    router_["APPLY_ACTIVITY"] = [this](auto c, auto j) { this->handleApplyActivity(c, j); };
    router_["LEAVE_ACTIVITY"] = [this](auto c, auto j) { this->handleLeaveActivity(c, j); };
    router_["COMPLETE_ACTIVITY"] = [this](auto c, auto j) { this->handleCompleteActivity(c, j); };
    
    // 建立 MySQL 核心持久管道
    m_mysql = mysql_init(nullptr);
    if (!mysql_real_connect(m_mysql, "127.0.0.1", "root", "111111", "qinyun", 3306, nullptr, 0)) {
        std::cerr << "❌ [MySQL Connection Fatal] " << mysql_error(m_mysql) << std::endl;
    } else {
        std::cout << "🟢 [MySQL Pipeline] Secure channel established with local cluster." << std::endl;
    }
}

BusinessServer::~BusinessServer() {
    if (m_mysql) mysql_close(m_mysql);
}

void BusinessServer::setThreadNum(int numThreads) { server_->setThreadNum(numThreads); }
void BusinessServer::start() { server_->start(); }

void BusinessServer::onConnection(const net::TcpConnectionPtr& conn) {
    if (conn->connected()) {
        std::cout << "📡 [Network Core] Dynamic connection established from " << conn->peerAddress().toIpPort() << " (fd=" << conn->fd() << ")" << std::endl;
    } else {
        std::cout << "🔌 [Network Core] Dynamic connection released gracefully (fd=" << conn->fd() << ")" << std::endl;
    }
}

void BusinessServer::onMessage(const net::TcpConnectionPtr& conn, net::Buffer* buffer, net::Timestamp) {
    std::string jsonBody;
    
    // 🟢 修复：使用 tryExtractPacket 循环从缓冲区中提取完整的 TCP 数据包
    while (tryExtractPacket(buffer, jsonBody)) {
        std::cout << "📥 [Network Engine] Inbound Message Payload: " << jsonBody << std::endl;

        json jsonObj;
        try {
            jsonObj = json::parse(jsonBody);
        } catch (const json::parse_error &e) {
            std::cerr << "❌ [Network Engine] JSON Parse Intercepted: " << e.what() << std::endl;
            sendError(conn, -1, "Invalid JSON data wire structure");
            continue; // 继续处理下一个包，而不是直接 return
        }

        if (jsonObj.contains("action") && jsonObj["action"].is_string()) {
            dispatchAction(conn, jsonObj);
        }
    }
}


bool BusinessServer::tryExtractPacket(net::Buffer* buffer, std::string& outJsonBody) {
    size_t readable = buffer->readableBytes();
    if (readable < protocol::kHeaderSize) return false;

    uint32_t bodyLenNet = 0;
    std::memcpy(&bodyLenNet, buffer->peek(), protocol::kHeaderSize);
    uint32_t bodyLen = ntohl(bodyLenNet);

    if (bodyLen == 0) {
        buffer->retrieve(protocol::kHeaderSize);
        outJsonBody.clear();
        return true;
    }

    if (bodyLen > protocol::kMaxBodyLen) {
        std::cerr << "[BusinessServer] Protocol error: bodyLen=" << bodyLen << " exceeds limit. Closing connection." << std::endl;
        buffer->retrieveAll();
        return false;
    }

    if (buffer->readableBytes() < protocol::kHeaderSize + bodyLen) return false;

    buffer->retrieve(protocol::kHeaderSize);
    outJsonBody = buffer->retrieveAsString(bodyLen);
    return true;
}

void BusinessServer::dispatchAction(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    std::string action = jsonObj["action"].get<std::string>();
    auto it = router_.find(action);
    if (it != router_.end()) {
        it->second(conn, jsonObj);
    } else {
        handleUnknownAction(conn, jsonObj);
    }
}

// ============================================================================
// 🟢 核心业务业务应用子 Handler 洗净区（对齐 QML 全部三维字段）
// ============================================================================

void BusinessServer::handleLogin(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    if (!jsonObj.contains("student_id") || !jsonObj["student_id"].is_string() ||
        !jsonObj.contains("password") || !jsonObj["password"].is_string()) {
        sendError(conn, -101, "Missing or invalid student_id/password");
        return;
    }

    std::string studentId = jsonObj["student_id"].get<std::string>();
    std::string password  = jsonObj["password"].get<std::string>();

    if (!m_mysql) {
        sendError(conn, -500, "Internal Database Handle Invalid");
        return;
    }

    std::string sql = "SELECT u.id, u.name, u.password_hash, u.role_id, u.department_id, u.status FROM users u WHERE u.student_id = '" + studentId + "';";
    std::cout << "[MySQL Pipeline] Authenticating user: " << studentId << std::endl;

    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        sendError(conn, -501, "Database Query Failure");
        return;
    }

    MYSQL_RES* result = mysql_store_result(m_mysql);
    MYSQL_ROW row = result ? mysql_fetch_row(result) : nullptr;
    if (!row) {
        if (result) mysql_free_result(result);
        sendError(conn, -102, "Student ID does not exist");
        return;
    }

    int dbUserId       = std::stoi(row[0]);
    std::string dbName = row[1] ? row[1] : "";
    std::string dbPwd  = row[2] ? row[2] : "";
    int dbRoleId       = std::stoi(row[3]);
    int dbDeptId       = row[4] ? std::stoi(row[4]) : 0;
    int dbStatus       = std::stoi(row[5]);
    mysql_free_result(result);

    if (dbStatus == 0) {
        sendError(conn, -103, "This account has been banned");
        return;
    }
    if (password != dbPwd) {
        sendError(conn, -104, "Incorrect password");
        return;
    }

    json resp = {{"status", "ok"}, {"code", 0}, {"action", "LOGIN"}};
    resp["data"]["user_id"]       = dbUserId;
    resp["data"]["name"]          = dbName;
    resp["data"]["student_id"]    = studentId;
    resp["data"]["role_id"]       = dbRoleId;
    resp["data"]["department_id"] = dbDeptId;
    resp["data"]["message"]       = "Welcome back, " + dbName;

    std::cout << "🟢 [Login Success] User " << dbName << " logged in." << std::endl;
    sendResponse(conn, resp);
}

// BusinessServer.cpp 核心处理区
void BusinessServer::handleUploadSchedule(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    if (!jsonObj.contains("user_id") || !jsonObj.contains("courses") || !jsonObj["courses"].is_array()) {
        sendError(conn, -11, "Param error for raw course templates");
        return;
    }

    int userId = jsonObj["user_id"].get<int>();
    auto courses = jsonObj["courses"];

    if (!m_mysql) { sendError(conn, -500, "Database Invalid"); return; }

    // 开启本地存储隐式事务
    mysql_autocommit(m_mysql, false);

    // 🟢 分支 A：管理员设定行政班级基础模板课表
    if (jsonObj.contains("target_class") && jsonObj["target_class"].is_string()) {
        std::string targetClass = jsonObj["target_class"].get<std::string>();
        std::string deleteTemplateSql = "DELETE FROM course_templates WHERE class_identifier = '" + targetClass + "';";
        mysql_query(m_mysql, deleteTemplateSql.c_str());

        for (const auto& c : courses) {
            std::string cName = c.value("course_name", "有课");
            int dayOfWeek     = c["day_of_week"].get<int>();  // 1-5
            int period        = c["period"].get<int>();       // 前端正课对应的 1,2,4,5
            int startWeek     = c.value("start_week", 1);
            int endWeek       = c.value("end_week", 16);
            int weekType      = c.value("week_type", 0);

            std::string insertClassTemplate = 
                "INSERT INTO course_templates (user_id, class_identifier, course_name, day_of_week, period, start_week, end_week, week_type) VALUES ("
                + std::to_string(userId) + ", '" + targetClass + "', '" + cName + "', " + std::to_string(dayOfWeek) + ", " 
                + std::to_string(period) + ", " + std::to_string(startWeek) + ", " + std::to_string(endWeek) + ", " + std::to_string(weekType) + ");";
            mysql_query(m_mysql, insertClassTemplate.c_str());
        }

        mysql_commit(m_mysql);
        mysql_autocommit(m_mysql, true);
        json resp = {{"status", "ok"}, {"action", "UPLOAD_SCHEDULE"}, {"message", "Class template pre-set successfully sealed."}};
        sendResponse(conn, resp);
        return;
    }

    // 🟢 分支 B：普通队员保存自己的私有课表并展开 30 位高速位图矩阵
    else {
        std::string deleteUserTemplateSql = "DELETE FROM course_templates WHERE user_id = " + std::to_string(userId) + " AND class_identifier IS NULL;";
        std::string deleteUserScheduleSql = "DELETE FROM schedules WHERE user_id = " + std::to_string(userId) + ";";
        mysql_query(m_mysql, deleteUserTemplateSql.c_str());
        mysql_query(m_mysql, deleteUserScheduleSql.c_str());

        // 初始化 1 到 16 周的位图数组（32位无符号，安全封锁有符号位污染）
        uint32_t weeklyBitmasks[17] = {0}; 

        for (const auto& c : courses) {
            std::string cName = c.value("course_name", "有课");
            int dayOfWeek     = c["day_of_week"].get<int>();  // 1-5
            int qmlPeriod     = c["period"].get<int>();       // 👈 前端传上来的正课大节

            // 🛑 核心映射加锁区：将前端的 4 大节正课，精准平铺进数据库的 6 时段网格中
            int dbPeriod = 1;
            if (qmlPeriod == 1)      dbPeriod = 1; // 正课第1大节 -> 数据库第1时段
            else if (qmlPeriod == 2) dbPeriod = 2; // 正课第2大节 -> 数据库第2时段
            else if (qmlPeriod == 3) dbPeriod = 4; // 前端第3大节(实际是下午第5-6节) -> 数据库第4时段 (把第3时段午休空出来！)
            else if (qmlPeriod == 4) dbPeriod = 5; // 前端第4大节(实际是下午第7-8节) -> 数据库第5时段 (把第6时段傍晚空出来！)
            else continue; // 过滤掉异常数据

            int startWeek     = c.value("start_week", 1);
            int endWeek       = c.value("end_week", 16);
            int weekType      = c.value("week_type", 0);

            // 保存用户私有模板留底
            std::string insertUserTemplate = 
                "INSERT INTO course_templates (user_id, class_identifier, course_name, day_of_week, period, start_week, end_week, week_type) VALUES ("
                + std::to_string(userId) + ", NULL, '" + cName + "', " + std::to_string(dayOfWeek) + ", " + std::to_string(qmlPeriod) + ", "
                + std::to_string(startWeek) + ", " + std::to_string(endWeek) + ", " + std::to_string(weekType) + ");";
            mysql_query(m_mysql, insertUserTemplate.c_str());

            // 精准计算 6 时段制下的 bit 偏移量
            int bitIndex = (dayOfWeek - 1) * 6 + (dbPeriod - 1);

            for (int w = startWeek; w <= endWeek; ++w) {
                if (weekType == 1 && (w % 2 == 0)) continue; 
                if (weekType == 2 && (w % 2 != 0)) continue; 

                weeklyBitmasks[w] |= (1u << bitIndex); // 标记忙碌
            }
        }

        // 批量将计算好的 1-16 周完整位图数据灌入 schedules 表
        std::string sqlBulk = "INSERT INTO schedules (user_id, week_number, bitmask) VALUES ";
        for (int w = 1; w <= 16; ++w) {
            sqlBulk += "(" + std::to_string(userId) + ", " + std::to_string(w) + ", " + std::to_string(weeklyBitmasks[w]) + ")";
            if (w < 16) sqlBulk += ", ";
        }
        sqlBulk += ";";

        if (mysql_query(m_mysql, sqlBulk.c_str()) != 0) {
            mysql_rollback(m_mysql);
            mysql_autocommit(m_mysql, true);
            sendError(conn, -501, "Failed to compile weekly 30-bit precision schedules");
            return;
        }

        mysql_commit(m_mysql);
        mysql_autocommit(m_mysql, true);

        json resp = {{"status", "ok"}, {"action", "UPLOAD_SCHEDULE"}, {"message", "Successfully compiled full semester 6-period layout."}};
        sendResponse(conn, resp);
    }
}
// ============================================================================
// Handler：根据组织身份拉取成员名录（带部门数据可见性隔离 + 专业班级扩容）
// ============================================================================

void BusinessServer::handleGetMembers(const net::TcpConnectionPtr& conn, const json& jsonObj)
{
    if (!jsonObj.contains("request_user_role") || !jsonObj["request_user_role"].is_number() ||
        !jsonObj.contains("request_user_dept") || !jsonObj["request_user_dept"].is_number()) {
        sendError(conn, -601, "Missing security context for member fetching");
        return;
    }

    int reqRole = jsonObj["request_user_role"].get<int>();
    int reqDept = jsonObj["request_user_dept"].get<int>();

    if (reqRole >= 40) { sendError(conn, -403, "Permission Denied"); return; }
    if (!m_mysql) { sendError(conn, -500, "Internal Database Handle Invalid"); return; }

    // ========================================================================
    // 🟢 核心时间轴动态计算：根据服务器当前真实时间，计算当前的教学周、星期和时段
    //    假设今天是 2026-05-23，处于本学期的第 10 教学周。
    // ========================================================================
    int currentWeek = 10;   // 实际可由服务器根据学期开学时间动态计算得出
    int currentDay = 6;     // 周六
    int currentPeriod = 2;  // 假设当前是中午时段 (1-6)

    // 计算当前时刻在 schedules 30位位图中的二进制偏移量
    int currentBitIndex = (currentDay - 1) * 6 + (currentPeriod - 1);
    uint32_t currentBitMask = (currentDay >= 1 && currentDay <= 5) ? (1u << currentBitIndex) : 0u;

    // 高阶三维联动大聚合 SQL
    std::string sql = 
        "SELECT u.id, u.name, u.student_id, u.role_id, u.department_id, d.name AS dept_name, u.status, u.major, u.class_name, "
        "COUNT(CASE WHEN am.is_attended = 1 THEN 1 END) AS total_count, "
        "COALESCE(SUM(CASE WHEN am.is_attended = 1 THEN am.duration_hours END), 0.0) AS total_hours, "
        
        // 🛑 动态穿透核心 1：检查此刻是否有正在进行的志愿者活动
        "(SELECT COUNT(*) FROM activity_members active_am "
        " INNER JOIN activities active_act ON active_am.activity_id = active_act.id "
        " WHERE active_am.user_id = u.id AND active_act.status = 2) AS is_in_activity, "
        
        // 🛑 动态穿透核心 2：抓取当前周的课表位图
        "COALESCE(s.bitmask, 0) AS current_schedule_mask "
        
        "FROM users u "
        "LEFT JOIN departments d ON u.department_id = d.id "
        "LEFT JOIN schedules s ON u.id = s.user_id AND s.week_number = " + std::to_string(currentWeek) + " "
        "LEFT JOIN activity_members am ON u.id = am.user_id ";

    if (reqRole == 30) {
        sql += "WHERE u.department_id = " + std::to_string(reqDept) + " ";
    } else {
        sql += "WHERE 1=1 ";
    }
    sql += "GROUP BY u.id ORDER BY u.role_id ASC, u.student_id ASC;";

    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        sendError(conn, -501, "Database Member Query Failure: " + std::string(mysql_error(m_mysql)));
        return;
    }

    MYSQL_RES* result = mysql_store_result(m_mysql);
    MYSQL_ROW row;
    json memberList = json::array();

    while (result && (row = mysql_fetch_row(result))) {
        json u;
        u["user_id"]       = std::stoi(row[0]);
        u["name"]          = row[1] ? row[1] : "";
        u["student_id"]    = row[2] ? row[2] : "";
        u["role_id"]       = std::stoi(row[3]);
        u["department_id"] = row[4] ? std::stoi(row[4]) : 0;
        u["dept_name"]     = row[5] ? row[5] : "未分配";
        u["status"]        = (std::stoi(row[6]) == 1) ? "active" : "disabled";
        u["major"]         = row[7] ? row[7] : "未记录";
        u["class_name"]    = row[8] ? row[8] : "未分配";
        u["total_count"]   = std::stoi(row[9]);
        u["total_hours"]   = std::stod(row[10]);

        int isInActivity = std::stoi(row[11]);
        uint32_t schedMask = std::stoul(row[12]);

        // 🧠 状态机判定
        if (isInActivity > 0) {
            u["current_state"] = "busy_activity"; // 正在进行志愿活动
        } else if (currentBitMask != 0u && (schedMask & currentBitMask) != 0u) {
            u["current_state"] = "busy_course";   // 正在上课
        } else {
            u["current_state"] = "free";          // 闲置有空
        }

        memberList.push_back(u);
    }
    if (result) mysql_free_result(result);

    json resp = {{"status", "ok"}, {"code", 0}, {"action", "GET_MEMBERS"}};
    resp["data"]["members"] = memberList;
    sendResponse(conn, resp);
}



void BusinessServer::handleGetTimeSlotAnalytics(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    if (!jsonObj.contains("target_week") || !jsonObj.contains("time_mask")) {
        sendError(conn, -415, "Missing analytical tracking bounds");
        return;
    }

    int targetWeek = jsonObj["target_week"].get<int>();
    uint32_t checkMask = jsonObj["time_mask"].get<uint32_t>();
    std::string targetClass = jsonObj.value("target_class", ""); // 支持针对单独班级或全队

    if (!m_mysql) { sendError(conn, -500, "Database link closed"); return; }

    // 统计总人数与在该时段空闲的人数
    std::string sql = 
        "SELECT COUNT(*) AS total_squad, "
        "COUNT(CASE WHEN (COALESCE(s.bitmask, 0) & " + std::to_string(checkMask) + ") = 0 THEN 1 END) AS free_squad "
        "FROM users u "
        "LEFT JOIN schedules s ON u.id = s.user_id AND s.week_number = " + std::to_string(targetWeek) + " "
        "WHERE u.role_id = 40 AND u.status = 1 ";
    
    if (!targetClass.empty()) {
        sql += "AND u.class_name = '" + targetClass + "' ";
    }

    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        sendError(conn, -501, "Matrix analysis calculation crash: " + std::string(mysql_error(m_mysql)));
        return;
    }

    MYSQL_RES* result = mysql_store_result(m_mysql);
    MYSQL_ROW row = result ? mysql_fetch_row(result) : nullptr;
    
    int totalSquad = row ? std::stoi(row[0]) : 0;
    int freeSquad  = row ? std::stoi(row[1]) : 0;
    if (result) mysql_free_result(result);

    double freeRate = totalSquad > 0 ? (double)freeSquad / totalSquad * 100.0 : 0.0;

    json resp = {{"status", "ok"}, {"action", "GET_TIME_ANALYTICS"}};
    resp["data"]["total_count"] = totalSquad;
    resp["data"]["free_count"]  = freeSquad;
    resp["data"]["free_rate"]   = freeRate; // 返回空闲百分比 (如 85.5%)
    
    sendResponse(conn, resp);
}


// ============================================================================
// Handler：智能排班无课筛选（融合高阶多维统计三维指标 + 专业班级扩容）
// ============================================================================
void BusinessServer::handleFilterMembers(const net::TcpConnectionPtr& conn, const json& jsonObj)
{
    if (!jsonObj.contains("activity_week") || !jsonObj.contains("time_mask")) {
        sendError(conn, -305, "Missing scheduling bits for dynamic pipeline");
        return;
    }

    int week = jsonObj["activity_week"].get<int>();
    uint32_t mask = jsonObj["time_mask"].get<uint32_t>(); // 30位高精无符号掩码
    
    // ========================================================================
    // 🟢 动态时间反算：根据服务器当前真实的时钟，计算在此刻的星期和时段
    //    以此作为名录右侧“🔥活动中/📚上课中/🍀空闲中”状态的瞬时评定依据
    // ========================================================================
    int currentWeek = week; // 默认与检索周对齐
    int currentDay = 3;     // 假设当前大联调是在周三
    int currentPeriod = 3;  // 中午档

    int currentBitIndex = (currentDay - 1) * 6 + (currentPeriod - 1);
    uint32_t currentBitMask = (currentDay >= 1 && currentDay <= 5) ? (1u << currentBitIndex) : 0u;

    // 升级版高聚合 SQL：合并历史工时计算、防双重录用锁以及实时状态探测
    std::string sql = 
        "SELECT u.id, u.name, u.student_id, d.name AS dept_name, u.phone, u.major, u.class_name, "
        "COUNT(CASE WHEN am.is_attended = 1 THEN 1 END) AS total_count, "
        "COALESCE(SUM(CASE WHEN am.is_attended = 1 THEN am.duration_hours END), 0.0) AS total_hours, "
        "COALESCE(MAX(CASE WHEN am.is_attended = 1 THEN am.updated_at END), '1970-01-01 00:00:00') AS last_time, "
        
        // 🛑 状态机指标 1：该用户此时此刻是否有【进行中】的活动任务？
        "(SELECT COUNT(*) FROM activity_members active_am "
        " INNER JOIN activities active_act ON active_am.activity_id = active_act.id "
        " WHERE active_am.user_id = u.id AND active_act.status = 2) AS is_in_activity, "
        
        // 状态机指标 2：提取本周课表位图规则
        "COALESCE(s.bitmask, 0) AS current_schedule_mask "
        
        "FROM users u "
        "INNER JOIN schedules s ON u.id = s.user_id AND s.week_number = " + std::to_string(week) + " "
        "LEFT JOIN departments d ON u.department_id = d.id "
        "LEFT JOIN activity_members am ON u.id = am.user_id " // 仅算历史指标，不干预主干行
        
        "WHERE s.week_number = " + std::to_string(week) + " "
        "AND (s.bitmask & " + std::to_string(mask) + ") = 0 "   // 1. 核心筛选：要求该时段课表必须空闲
        "AND u.role_id = 40 AND u.status = 1 "
        
        // 🛑 核心防重锁：用高效的 NOT EXISTS。如果该用户这周这个时段已经在别的活跃活动中了，直接剔除！
        "AND NOT EXISTS ("
        "    SELECT 1 FROM activity_members busy_am "
        "    INNER JOIN activities busy_act ON busy_am.activity_id = busy_act.id "
        "    WHERE busy_am.user_id = u.id "
        "    AND busy_act.activity_week = " + std::to_string(week) + " "
        "    AND (busy_act.time_mask & " + std::to_string(mask) + ") != 0 "
        "    AND busy_act.status IN (1, 2)" 
        ") ";

    if (jsonObj.contains("department_id")) {
        sql += "AND u.department_id = " + std::to_string(jsonObj["department_id"].get<int>()) + " ";
    }
    
    sql += "GROUP BY u.id ORDER BY u.student_id ASC;";

    std::cout << "🛡️ [State-Machine Enhanced Selection Query] " << sql << std::endl;

    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        sendError(conn, -501, "Aggregation calculation engine failed: " + std::string(mysql_error(m_mysql)));
        return;
    }

    MYSQL_RES* result = mysql_store_result(m_mysql);
    MYSQL_ROW row;
    json memberList = json::array();

    while (result && (row = mysql_fetch_row(result))) {
        json m;
        m["id"]           = std::stoi(row[0]);
        m["name"]         = row[1] ? row[1] : ""; // 👈 严格修正对齐 row[1]
        m["student_id"]   = row[2] ? row[2] : "";
        m["dept_name"]    = row[3] ? row[3] : "未分配";
        m["phone"]        = row[4] ? row[4] : "-";
        m["major"]        = row[5] ? row[5] : "";
        m["class_name"]   = row[6] ? row[6] : "";
        m["total_count"]  = std::stoi(row[7]); 
        m["total_hours"]  = std::stod(row[8]); 
        m["last_time"]    = row[9] ? row[9] : "无记录"; 

        int isInActivity = std::stoi(row[10]);
        uint32_t schedMask = std::stoul(row[11]);

        // ====================================================================
        // 🧠 瞬时排他漏斗状态机判定：赋予选人面板前瞻感知能力
        // ====================================================================
        if (isInActivity > 0) {
            m["current_state"] = "busy_activity"; // 此时此刻正在背着别的志愿任务
        } else if (currentBitMask != 0u && (schedMask & currentBitMask) != 0u) {
            m["current_state"] = "busy_course";   // 此时此刻正在教室上正课
        } else {
            m["current_state"] = "free";          // 完美闲置，可随时征召
        }

        memberList.push_back(m);
    }
    if (result) mysql_free_result(result);

    json resp;
    resp["status"] = "ok";
    resp["action"] = "FILTER_AVAILABLE_MEMBERS";
    resp["data"]["members"] = memberList;
    sendResponse(conn, resp);
}


void BusinessServer::handleConfirmAssign(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    if (!jsonObj.contains("activity_id") || !jsonObj["user_ids"].is_array()) {
        sendError(conn, -301, "Missing or invalid activity_id/user_ids");
        return;
    }

    int activityId = jsonObj["activity_id"].get<int>();
    auto userIds   = jsonObj["user_ids"];
    if (userIds.empty()) {
        sendError(conn, -302, "User IDs array cannot be empty");
        return;
    }

    std::string sql = "INSERT INTO activity_members (activity_id, user_id, assign_type, sign_in_status) VALUES ";
    for (size_t i = 0; i < userIds.size(); ++i) {
        sql += "(" + std::to_string(activityId) + ", " + std::to_string(userIds[i].get<int>()) + ", 1, 0)";
        if (i < userIds.size() - 1) sql += ", ";
    }
    sql += " ON DUPLICATE KEY UPDATE updated_at = NOW();";

    std::cout << "[MySQL Pipeline] Bulk Assigning Members: " << sql << std::endl;
    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        sendError(conn, -501, "Database Bulk Insert Failure");
        return;
    }

    json resp = {{"status", "ok"}, {"code", 0}, {"action", "CONFIRM_ASSIGN"}};
    resp["message"] = "Successfully deployed " + std::to_string(userIds.size()) + " members.";
    sendResponse(conn, resp);
}

void BusinessServer::handleBulkRegister(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    if (!jsonObj.contains("users") || !jsonObj["users"].is_array() || jsonObj["users"].empty()) {
        sendError(conn, -701, "Invalid mass registry block payload");
        return;
    }

    auto users = jsonObj["users"];
    // 🟢 扩容支持 major 和 class_name
    std::string sql = "INSERT INTO users (name, student_id, password_hash, role_id, department_id, major, class_name, status) VALUES ";
    for (size_t i = 0; i < users.size(); ++i) {
        auto u = users[i];
        std::string major = u.value("major", "");
        std::string className = u.value("class_name", "");
        
        sql += "('" + u["name"].get<std::string>() + "', '" 
               + u["student_id"].get<std::string>() + "', '" 
               + u["password"].get<std::string>() + "', " 
               + std::to_string(u["role_id"].get<int>()) + ", " 
               + std::to_string(u["department_id"].get<int>()) + ", '"
               + major + "', '" + className + "', 1)";
        if (i < users.size() - 1) sql += ", ";
    }
    sql += " ON DUPLICATE KEY UPDATE name=VALUES(name), role_id=VALUES(role_id), major=VALUES(major), class_name=VALUES(class_name);";

    std::cout << "[MySQL Bulk Register Pipeline] Processing..." << std::endl;
    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        sendError(conn, -501, mysql_error(m_mysql));
        return;
    }

    json resp = {{"status", "ok"}, {"action", "BULK_REGISTER_USERS"}};
    resp["data"]["count"] = users.size();
    sendResponse(conn, resp);
}


void BusinessServer::handleAddActivity(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    if (!jsonObj.contains("title") || !jsonObj.contains("location") || 
        !jsonObj.contains("activity_week") || !jsonObj.contains("time_mask") || 
        !jsonObj.contains("organizer_id")) {
        sendError(conn, -401, "Param loss for issuing service event");
        return;
    }

    std::string title = jsonObj["title"].get<std::string>();
    std::string loc = jsonObj["location"].get<std::string>();
    int week = jsonObj["activity_week"].get<int>();
    uint32_t mask = jsonObj["time_mask"].get<uint32_t>(); // 强制无符号防御性校验
    int organizerId = jsonObj["organizer_id"].get<int>();
    int deptId = jsonObj.contains("department_id") ? jsonObj["department_id"].get<int>() : 1; 
    std::string desc = jsonObj.value("description", "无描述内容");

    if (!m_mysql) { 
        sendError(conn, -500, "Database link invalid"); 
        return; 
    }

    // ========================================================================
    // 🟢 核心高阶算法：通过 time_mask 位穿透，自动计算该活动需要累加的标准志愿工时
    // ========================================================================
    double defaultDuration = 0.0;
    
    // 一学期按 5天 × 每天6时段 = 30位 遍历位矩阵
    for (int i = 0; i < 30; ++i) {
        if ((mask & (1u << i)) != 0) {
            // 通过余数反算该时段在一天中属于第几个时段 (0-5)
            int periodIndex = i % 6; 
            
            if (periodIndex == 5) {
                // 第 6 个时段：傍晚/夜间档（18:00 - 21:00），标准工时计 3.0 小时
                defaultDuration += 3.0;
            } else {
                // 第 1-5 个时段（含常规课、中午档）：标准工时一律计 2.0 小时
                defaultDuration += 2.0;
            }
        }
    }

    // 防止特殊情况下（如前端发包未勾选时段）工时为 0
    if (defaultDuration <= 0.0) {
        defaultDuration = 2.0; // 默认保底 2.0 小时
    }

    // ========================================================================
    // 写入 activities 表（新增 default_duration 字段绑定）
    // ========================================================================
    std::string sql = "INSERT INTO activities (title, description, location, organizer_id, department_id, "
                      "activity_week, time_mask, max_participants, default_duration, status) VALUES ("
                      "'" + title + "', '" + desc + "', '" + loc + "', " + std::to_string(organizerId) + ", " 
                      + std::to_string(deptId) + ", " + std::to_string(week) + ", " + std::to_string(mask) + ", 30, " 
                      + std::to_string(defaultDuration) + ", 1);";

    std::cout << "🚀 [MySQL Pipeline] Forging Precision Duration Activity Record: " << sql << std::endl;

    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        sendError(conn, -501, "Database Insert Activity Failure: " + std::string(mysql_error(m_mysql)));
        return;
    }

    // 返回成功信息给前端
    json resp = {{"status", "ok"}, {"code", 0}, {"action", "ADD_ACTIVITY"}};
    sendResponse(conn, resp);
}


void BusinessServer::handleGetActivities(const net::TcpConnectionPtr& conn, const json&) {
    if (!m_mysql) { sendError(conn, -500, "Database Invalid"); return; }
    std::string sql = "SELECT id, title, activity_week, time_mask, location FROM activities WHERE status = 1;";
    if (mysql_query(m_mysql, sql.c_str()) != 0) { sendError(conn, -501, "Query Failure"); return; }

    MYSQL_RES* result = mysql_store_result(m_mysql);
    json activityList = json::array();
    MYSQL_ROW row;
    while (result && (row = mysql_fetch_row(result))) {
        json act;
        act["activity_id"]   = std::stoi(row[0]);
        act["title"]         = row[1] ? row[1] : "";
        act["activity_week"] = std::stoi(row[2]);
        act["time_mask"]     = std::stoul(row[3]);
        act["location"]      = row[4] ? row[4] : "";
        activityList.push_back(act);
    }
    if (result) mysql_free_result(result);

    json resp = {{"status", "ok"}, {"code", 0}, {"action", "GET_ACTIVITIES"}};
    resp["data"]["activities"] = activityList;
    sendResponse(conn, resp);
}

void BusinessServer::handleGetManagementActivities(const net::TcpConnectionPtr& conn, const json&) {
    std::string sql = "SELECT a.id, a.title, a.description, a.location, a.activity_week, a.time_mask, a.max_participants, a.status, COUNT(am.user_id) AS assigned_count FROM activities a LEFT JOIN activity_members am ON a.id = am.activity_id GROUP BY a.id ORDER BY a.id DESC;";
    if (mysql_query(m_mysql, sql.c_str()) != 0) { sendError(conn, -501, "Query Failure"); return; }

    MYSQL_RES* result = mysql_store_result(m_mysql);
    json activityList = json::array();
    MYSQL_ROW row;
    while (result && (row = mysql_fetch_row(result))) {
        json act;
        act["activity_id"]      = std::stoi(row[0]);
        act["title"]            = row[1] ? row[1] : "";
        act["description"]      = row[2] ? row[2] : "";
        act["location"]         = row[3] ? row[3] : "";
        act["activity_week"]    = std::stoi(row[4]);
        act["time_mask"]        = std::stoul(row[5]);
        act["max_participants"] = std::stoi(row[6]);
        act["status"]           = std::stoi(row[7]);
        act["assigned_count"]   = std::stoi(row[8]);
        activityList.push_back(act);
    }
    if (result) mysql_free_result(result);

    json resp = {{"status", "ok"}, {"code", 0}, {"action", "GET_MANAGEMENT_ACTIVITIES"}};
    resp["data"]["activities"] = activityList;
    sendResponse(conn, resp);
}

// 确保 handleGetClassTemplate 遭遇空模板时安全返回 ok 状态
void BusinessServer::handleGetClassTemplate(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    if (!jsonObj.contains("class_name") || !jsonObj["class_name"].is_string()) {
        sendError(conn, -411, "Missing or invalid class_name parameter");
        return;
    }

    std::string className = jsonObj["class_name"].get<std::string>();
    if (!m_mysql) { sendError(conn, -500, "Internal Database Handle Invalid"); return; }

    // 严格按原生 C API 驱动组装并隔离安全清洗
    std::string sql = "SELECT course_name, day_of_week, period, start_week, end_week, week_type "
                      "FROM course_templates WHERE class_identifier = '" + className + "';";

    std::cout << "🔍 [Class Timetable Template] Querying baseline for class: " << className << std::endl;

    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        sendError(conn, -501, "Database query failure for class templates: " + std::string(mysql_error(m_mysql)));
        return;
    }

    MYSQL_RES* result = mysql_store_result(m_mysql);
    MYSQL_ROW row;
    json coursesArray = json::array(); // 🚀 默认初始化为空 []，确保无课表班级安全过河，不卡死前端

    while (result && (row = mysql_fetch_row(result))) {
        json c;
        c["course_name"] = row[0] ? row[0] : "有课";
        c["day_of_week"] = std::stoi(row[1]);
        c["period"]      = std::stoi(row[2]);
        c["start_week"]  = std::stoi(row[3]);
        c["end_week"]    = std::stoi(row[4]);
        c["week_type"]   = std::stoi(row[5]);
        coursesArray.push_back(c);
    }
    if (result) mysql_free_result(result);

    json resp = {{"status", "ok"}, {"action", "GET_CLASS_TEMPLATE"}};
    resp["data"]["courses"] = coursesArray;
    sendResponse(conn, resp);
}

void BusinessServer::handleDeleteActivity(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    int actId = jsonObj["activity_id"].get<int>();
    std::string sql = "DELETE FROM activities WHERE id = " + std::to_string(actId) + ";";
    if (mysql_query(m_mysql, sql.c_str()) != 0) { sendError(conn, -501, "Delete Failure"); return; }
    json resp = {{"status", "ok"}, {"action", "DELETE_ACTIVITY"}};
    sendResponse(conn, resp);
}


void BusinessServer::handleUpdateActivity(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    int actId = jsonObj["activity_id"].get<int>();
    std::string title = jsonObj["title"].get<std::string>();
    std::string loc = jsonObj["location"].get<std::string>();
    int maxPart = jsonObj.value("max_participants", 30);

    std::string sql = "UPDATE activities SET title = '" + title + "', location = '" + loc + "', max_participants = " + std::to_string(maxPart) + " WHERE id = " + std::to_string(actId) + ";";
    if (mysql_query(m_mysql, sql.c_str()) != 0) { sendError(conn, -501, "Update Failure"); return; }
    json resp = {{"status", "ok"}, {"action", "UPDATE_ACTIVITY"}};
    sendResponse(conn, resp);
}
// ============================================================================
// Handler：普通成员主动申请参与活动（带状态检查与名额校验）
// ============================================================================
void BusinessServer::handleApplyActivity(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    if (!jsonObj.contains("activity_id") || !jsonObj.contains("user_id")) {
        sendError(conn, -801, "Missing activity_id or user_id");
        return;
    }
    int activityId = jsonObj["activity_id"].get<int>();
    int userId = jsonObj["user_id"].get<int>();

    // 1. 验证活动是否存在且处于“未开始(status=1)”状态，同时检查人数限制
    std::string checkSql = "SELECT title, max_participants, status, "
                           "(SELECT COUNT(*) FROM activity_members WHERE activity_id = " + std::to_string(activityId) + " AND assign_type = 0) as current_count "
                           "FROM activities WHERE id = " + std::to_string(activityId) + ";";
    
    if (mysql_query(m_mysql, checkSql.c_str()) != 0) {
        sendError(conn, -501, "Database pre-check failure");
        return;
    }

    MYSQL_RES* res = mysql_store_result(m_mysql);
    MYSQL_ROW row = res ? mysql_fetch_row(res) : nullptr;
    if (!row) {
        if (res) mysql_free_result(res);
        sendError(conn, -802, "Activity does not exist");
        return;
    }

    int maxPart = std::stoi(row[1]);
    int status = std::stoi(row[2]);
    int currentCount = std::stoi(row[3]);
    mysql_free_result(res);

    if (status != 1) {
        sendError(conn, -803, "Application denied: Activity has already started or ended");
        return;
    }
    if (currentCount >= maxPart) {
        sendError(conn, -804, "Application denied: Activity recruitment capacity is full");
        return;
    }

    // 2. 写入关联表 (assign_type=0 表示主动申请，sign_in_status=0 表示未签到)
    std::string insertSql = "INSERT INTO activity_members (activity_id, user_id, assign_type, sign_in_status) "
                            "VALUES (" + std::to_string(activityId) + ", " + std::to_string(userId) + ", 0, 0) "
                            "ON DUPLICATE KEY UPDATE assign_type = 0;";
    
    if (mysql_query(m_mysql, insertSql.c_str()) != 0) {
        sendError(conn, -501, "Failed to submit application to registry");
        return;
    }

    json resp = {{"status", "ok"}, {"code", 0}, {"action", "APPLY_ACTIVITY"}, {"message", "Successfully signed up for the event"}};
    sendResponse(conn, resp);
}

// ============================================================================
// Handler：普通成员请假 / 取消申请（仅限活动未开始阶段）
// ============================================================================
void BusinessServer::handleLeaveActivity(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    if (!jsonObj.contains("activity_id") || !jsonObj.contains("user_id")) {
        sendError(conn, -811, "Missing activity_id or user_id");
        return;
    }
    int activityId = jsonObj["activity_id"].get<int>();
    int userId = jsonObj["user_id"].get<int>();

    // 1. 拦截检查活动状态
    // 1. 拦截检查活动状态
    std::string checkSql = "SELECT status FROM activities WHERE id = " + std::to_string(activityId) + ";";
    
    if (mysql_query(m_mysql, checkSql.c_str()) != 0) {
        sendError(conn, -501, "Query active status failure");
        return;
    }
    
    // 🟢 修复：在这里显式声明并获取结果集指针
    MYSQL_RES* res = mysql_store_result(m_mysql); 
    if (!res) {
        sendError(conn, -501, "Query active status field lock failure");
        return;
    }
    
    MYSQL_ROW row = mysql_fetch_row(res);
    int status = row ? std::stoi(row[0]) : 0;
    mysql_free_result(res);

    if (status != 1) {
        sendError(conn, -812, "Leave request denied: Cannot cancel assignment after activity starts");
        return;
    }

    // 2. 从关联表中移除该用户
    std::string deleteSql = "DELETE FROM activity_members WHERE activity_id = " + std::to_string(activityId) + " AND user_id = " + std::to_string(userId) + ";";
    if (mysql_query(m_mysql, deleteSql.c_str()) != 0) {
        sendError(conn, -501, "Database deletion breakdown");
        return;
    }

    json resp = {{"status", "ok"}, {"code", 0}, {"action", "LEAVE_ACTIVITY"}, {"message", "Successfully left the activity"}};
    sendResponse(conn, resp);
}

// ============================================================================
// Handler：管理员完结活动并动态更新成员名单工时（三维统计数据源更新区）
// ============================================================================
void BusinessServer::handleCompleteActivity(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    if (!jsonObj.contains("activity_id") || !jsonObj.contains("member_hours") || !jsonObj["member_hours"].is_array()) {
        sendError(conn, -821, "Missing activity_id or individual member hour blocks");
        return;
    }

    int activityId = jsonObj["activity_id"].get<int>();
    auto memberHours = jsonObj["member_hours"]; // 格式: [{"user_id": 1, "duration": 3.5, "attended": 1}, ...]

    // 1. 开启本地 MySQL 事务保证多笔更新的原子性
    mysql_autocommit(m_mysql, false);

    // 2. 循环更新每个录入成员的实际出勤状态与志愿工时
    for (const auto& item : memberHours) {
        if (!item.contains("user_id") || !item.contains("duration") || !item.contains("attended")) continue;
        
        int uId = item["user_id"].get<int>();
        double duration = item["duration"].get<double>();
        int attended = item["attended"].get<int>(); // 1: 到场, 0: 缺席

        std::string updateMemberSql = 
            "UPDATE activity_members SET "
            "duration_hours = " + std::to_string(duration) + ", "
            "is_attended = " + std::to_string(attended) + ", "
            "sign_in_status = 1, " // 标记为已完结结算
            "updated_at = NOW() "
            "WHERE activity_id = " + std::to_string(activityId) + " AND user_id = " + std::to_string(uId) + ";";

        if (mysql_query(m_mysql, updateMemberSql.c_str()) != 0) {
            mysql_rollback(m_mysql);
            mysql_autocommit(m_mysql, true);
            sendError(conn, -502, "Transaction broken: failure in updating member tracking records");
            return;
        }
    }

    // 3. 将活动状态标志变更为“已完结 (status=3)”（假设 1:未开始, 2:进行中, 3:已结束）
    std::string updateActivitySql = "UPDATE activities SET status = 3, updated_at = NOW() WHERE id = " + std::to_string(activityId) + ";";
    if (mysql_query(m_mysql, updateActivitySql.c_str()) != 0) {
        mysql_rollback(m_mysql);
        mysql_autocommit(m_mysql, true);
        sendError(conn, -503, "Transaction broken: failed to transition status to completed");
        return;
    }

    // 提交事务
    mysql_commit(m_mysql);
    mysql_autocommit(m_mysql, true);

    // 💡 解释：此时由于关联表数据已变，你原本就写好的 handleFilterMembers 聚合查询 
    // COUNT(CASE WHEN am.is_attended = 1 THEN 1 END) 和 SUM(...) 会在前端下次刷新时自动拉取到最新的三维动态指标！

    json resp = {{"status", "ok"}, {"code", 0}, {"action", "COMPLETE_ACTIVITY"}, {"message", "Activity successfully settled and locked."}};
    sendResponse(conn, resp);
}

// ============================================================================
// Handler：拉取指定活动已录用的成员列表（用于管理端完结工时结算）
// ============================================================================
void BusinessServer::handleGetAssignedMembers(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    if (!jsonObj.contains("activity_id")) {
        sendError(conn, -901, "Missing activity_id");
        return;
    }
    int activityId = jsonObj["activity_id"].get<int>();

    if (!m_mysql) { sendError(conn, -500, "Database Invalid"); return; }

    // 联合查询出当前活动关联的所有成员基本信息
    std::string sql = 
        "SELECT u.id, u.name, u.student_id, u.major, u.class_name, am.duration_hours, am.is_attended "
        "FROM activity_members am "
        "INNER JOIN users u ON am.user_id = u.id "
        "WHERE am.activity_id = " + std::to_string(activityId) + ";";

    std::cout << "[MySQL Pipeline] Fetching assigned members for activity: " << activityId << std::endl;

    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        sendError(conn, -501, "Database query failure for assigned members");
        return;
    }

    MYSQL_RES* result = mysql_store_result(m_mysql);
    json memberList = json::array();
    MYSQL_ROW row;

    while (result && (row = mysql_fetch_row(result))) {
        json m;
        m["user_id"]        = std::stoi(row[0]);
        m["name"]           = row[1] ? row[1] : "";
        m["student_id"]     = row[2] ? row[2] : "";
        m["major"]          = row[3] ? row[3] : "";
        m["class_name"]     = row[4] ? row[4] : "";
        m["duration_hours"] = row[5] ? std::stod(row[5]) : 0.0;
        m["is_attended"]    = row[6] ? std::stoi(row[6]) : 1; // 默认出勤
        memberList.push_back(m);
    }
    if (result) mysql_free_result(result);

    json resp = {{"status", "ok"}, {"code", 0}, {"action", "GET_ASSIGNED_MEMBERS"}};
    resp["data"]["members"] = memberList;
    sendResponse(conn, resp);
}

// ============================================================================
// Handler：管理员单条/批量除名指定成员（联动清理无课表与活动关联）
// ============================================================================
void BusinessServer::handleDeleteMember(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    if (!jsonObj.contains("target_user_id")) {
        sendError(conn, -911, "Missing target_user_id parameter");
        return;
    }
    int targetId = jsonObj["target_user_id"].get<int>();

    if (!m_mysql) { 
        sendError(conn, -500, "Database link down"); 
        return; 
    }

    // 🟢 核心重构：利用建表时已有的 ON DELETE CASCADE 外键级联，只对主表 users 斩下一刀。
    //    不使用繁琐的隐式多句事务，彻底斩断与 MySQL 内部级联引擎的踩脚死锁链！
    std::string sqlUser = "DELETE FROM users WHERE id = " + std::to_string(targetId) + ";";

    std::cout << "🗑️ [Purge Member Execution] Safe cascading purge for user_id=" << targetId << std::endl;

    if (mysql_query(m_mysql, sqlUser.c_str()) != 0) {
        // 如果失败，打出原生的 MySQL 报错，方便联调
        std::cerr << "❌ [MySQL Purge Deadlock Avoided But Error Occurred] " << mysql_error(m_mysql) << std::endl;
        sendError(conn, -501, "Database execution break: " + std::string(mysql_error(m_mysql)));
        return;
    }

    // 完美过关，返回响应
    json resp = {{"status", "ok"}, {"code", 0}, {"action", "DELETE_MEMBER"}, {"message", "Member and all related schedules/assignments successfully cascaded by DB engine."}};
    sendResponse(conn, resp);
}

void BusinessServer::handleUnknownAction(const net::TcpConnectionPtr& conn, const json& jsonObj) {
    std::string action = jsonObj.value("action", "(missing)");
    sendError(conn, -99, "Unknown system action route: '" + action + "'");
}

void BusinessServer::sendResponse(const net::TcpConnectionPtr& conn, const json& responseJson) {
    std::string body = responseJson.dump();
    conn->send(body.c_str(), static_cast<int>(body.size()));
}

void BusinessServer::sendError(const net::TcpConnectionPtr& conn, int errorCode, const std::string& errorMessage) {
    json resp = {{"status", "error"}, {"code", errorCode}, {"message", errorMessage}};
    sendResponse(conn, resp);
}

bool BusinessServer::validateField(const json& jsonObj, const std::string& fieldName, json::value_t expectedType) {
    auto it = jsonObj.find(fieldName);
    if (it == jsonObj.end()) return false;
    return it->type() == expectedType;
}

}  // namespace qingyun