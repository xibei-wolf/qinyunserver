// ============================================================================
// TimeConverter.cpp — 青云志愿服务队管理系统 · 核心时间控制模块落地实现
// ============================================================================

#include "TimeConverter.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <iostream>

namespace qingyun
{

    std::chrono::system_clock::time_point TimeConverter::stringToDate(const std::string& dateStr)
    {
        std::tm tm = {};
        std::istringstream ss(dateStr);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        // 显式拒绝夏令时干扰
        tm.tm_isdst = -1; 
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    std::string TimeConverter::dateToString(std::chrono::system_clock::time_point tp)
    {
        std::time_t timet = std::chrono::system_clock::to_time_t(tp);
        std::tm* tm = std::localtime(&timet);
        std::ostringstream oss;
        oss << std::put_time(tm, "%Y-%m-%d");
        return oss.str();
    }

    bool TimeConverter::convertDateToWeekAndDay(const std::string& termStartDateStr,
                                                const std::string& targetDateStr,
                                                int& outWeek, int& outDay)
    {
        try {
            auto termStart = stringToDate(termStartDateStr);
            auto target = stringToDate(targetDateStr);

            auto duration = target - termStart;
            // 转换为绝对天数差值
            int daysDiff = static_cast<int>(std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24);

            if (daysDiff < 0) {
                return false; // 活动日期早于开学日，判定非法
            }

            // 计算教学周：第 0-6 天为第 1 周，第 7-13 天为第 2 周...
            outWeek = (daysDiff / 7) + 1;

            // 计算星期几：开学日当天(周一) daysDiff%7 = 0 -> 星期一(1)
            // 周日时 daysDiff%7 = 6 -> 星期日(7)
            outDay = (daysDiff % 7) + 1;

            // 契合你数据库 chk_activity_week 的 1-20 周硬约束
            return (outWeek >= 1 && outWeek <= 20);
        }
        catch (...) {
            return false;
        }
    }

    uint32_t TimeConverter::calculateHourMask(const std::string& startTimeStr, const std::string& endTimeStr)
    {
        int startHour = 0, startMin = 0;
        int endHour = 0, endMin = 0;

        // 解析输入的时间字符串，兼容 "HH:MM:SS" 与 "HH:MM"
        std::sscanf(startTimeStr.c_str(), "%d:%d", &startHour, &startMin);
        std::sscanf(endTimeStr.c_str(), "%d:%d", &endHour, &endMin);

        uint32_t mask = 0;

        // 遍历 7:00 到 22:00 共 15 个一小时档位
        for (int i = 0; i < 15; ++i)
        {
            int slotStart = 7 + i;
            int slotEnd = slotStart + 1;

            // 判断当前 1 小时的网格与活动时间是否存在交集
            // 经典相交区间算法：活动开始时间 < 网格结束时间 && 活动结束时间 > 网格开始时间
            double actStart = startHour + (startMin / 60.0);
            double actEnd = endHour + (endMin / 60.0);

            if (actStart < slotEnd && actEnd > slotStart)
            {
                mask |= (1u << i); // 产生交集，该小时判定为忙碌/需要人手
            }
        }
        return mask;
    }

    uint32_t TimeConverter::convertQmlPeriodToDayMask(int qmlPeriod)
    {
        uint32_t mask = 0;
        switch (qmlPeriod)
        {
        case 1:
            mask |= (1u << 1); // 08:00 - 09:00
            mask |= (1u << 2); // 09:00 - 10:00
            break;
        case 2:
            mask |= (1u << 3); // 10:00 - 11:00
            mask |= (1u << 4); // 11:00 - 12:00
            break;
        case 3:
            mask |= (1u << 7); // 14:00 - 15:00
            mask |= (1u << 8); // 15:00 - 16:00
            break;
        case 4:
            mask |= (1u << 9);  // 16:00 - 17:00
            mask |= (1u << 10); // 17:00 - 18:00
            break;
        default:
            break;
        }
        return mask;
    }

    double TimeConverter::calculateDurationByMask(uint32_t hourMask)
    {
        double hours = 0.0;
        for (int i = 0; i < 15; ++i)
        {
            if ((hourMask & (1u << i)) != 0)
            {
                hours += 1.0; // 每一位代表1个小时
            }
        }
        return hours;
    }

} // namespace qingyun