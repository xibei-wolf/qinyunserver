// ============================================================================
// TimeConverter.h — 青云志愿服务队管理系统 · 全局高精度日历与时段位图换算器
// ============================================================================

#pragma once

#include <string>
#include <chrono>
#include <cstdint>

namespace qingyun
{
    class TimeConverter
    {
    public:
        // ========================================================================
        // 核心一：日历日期与教学周次换算
        // ========================================================================

        /**
         * @brief 将 YYYY-MM-DD 格式字符串转为 std::chrono 的时间点
         */
        static std::chrono::system_clock::time_point stringToDate(const std::string& dateStr);

        /**
         * @brief 将时间点转为 YYYY-MM-DD 格式字符串
         */
        static std::string dateToString(std::chrono::system_clock::time_point tp);

        /**
         * @brief 🌟 核心绝对换算算法：依据学期开学日，计算目标日期属于第几教学周、星期几
         * @param termStartDateStr 学期开学日期（格式: "2026-03-02"，建议为周一）
         * @param targetDateStr 目标活动日期（格式: "2026-05-24"）
         * @param outWeek 输出参数：计算得出的教学周 (1-20周)
         * @param outDay 输出参数：计算得出的星期几 (1:周一, 2:周二 ... 7:周日)
         * @return true 换算成功且在常规学期(1-20周)范围内；false 说明日期超出学期范围
         */
        static bool convertDateToWeekAndDay(const std::string& termStartDateStr,
                                            const std::string& targetDateStr,
                                            int& outWeek, int& outDay);

        // ========================================================================
        // 核心二：高精度 15 位小时掩码矩阵计算 (7:00 - 22:00)
        // ========================================================================

        /**
         * @brief 🌟 精准时段转换：将具体的绝对时间范围，转换为当天 15 位的忙碌/活动时间掩码
         * @details 网格定义：7:00-8:00 为 Bit 0, 8:00-9:00 为 Bit 1 ... 21:00-22:00 为 Bit 14
         * @param startTimeStr 开始时间（如 "08:30:00" 或 "08:00"）
         * @param endTimeStr 结束时间（如 "11:45:00" 或 "12:00"）
         * @return uint32_t 返回生成的 15 位单日时间掩码（高位填充0）
         */
        static uint32_t calculateHourMask(const std::string& startTimeStr, const std::string& endTimeStr);

        /**
         * @brief 🌟 课表兼容平铺：将前端传来的传统教务处4大节正课，精准映射到 15 位单日全天候掩码中
         * @param qmlPeriod 前端传统大节号 (1, 2, 3, 4)
         * @details
         *   - 1大节 (08:00-09:50) -> 占用 Bit 1, Bit 2 (8:00-10:00)
         *   - 2大节 (10:10-12:00) -> 占用 Bit 3, Bit 4 (10:00-12:00)
         *   - 3大节 (14:10-16:00) -> 占用 Bit 7, Bit 8 (14:00-16:00)
         *   - 4大节 (16:10-17:50) -> 占用 Bit 9, Bit 10 (16:00-18:00)
         *   - 完美留空：Bit 0(7点档), Bit 5-6(午休12-14点档), Bit 11-14(晚间18-22点档)
         */
        static uint32_t convertQmlPeriodToDayMask(int qmlPeriod);

        /**
         * @brief 根据 15 位小时掩码自动结算标称的志愿工时（每占1位计1小时）
         */
        static double calculateDurationByMask(uint32_t hourMask);
    };
} // namespace qingyun