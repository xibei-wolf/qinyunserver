#pragma once

#include <cstdint>
#include <time.h>
#include <string>

namespace net
{
    class Timestamp
    {
    public:
        Timestamp(); // 构造⽆效的时间戳对象 0
        explicit Timestamp(int64_t usec);
        // ⼩于等于0则表⽰⽆效
        bool valid() const;
        void swap(Timestamp &other);
        // 返回时间戳的值（微秒）
        int64_t microSecondsSinceEpoch() const;
        // 返回时间戳的值（秒）
        time_t secondsSinceEpoch() const;
        // 将时间戳转换并返回⼀个时间戳对象
        static Timestamp fromUnixTime(time_t t);
        static Timestamp fromUnixTime(time_t t, int microseconds);
        // 转换为 yyyy-mm-dd hh:mm:ss gmtime_r
        std::string toFormattedString();
        // 获取系统时间，并转为Timestamp对象， gettimeofday()
        static Timestamp now();
        // 返回⼀个⽆效的时间戳对象, 也就是默认的Timestamp构造对象
        static Timestamp invalid();
        static const int kMicroSecondsPerSecond = 1000 * 1000;

    private:
        int64_t _microSecondsSinceEpoch; // 微秒为单位
    };

    inline bool operator<(Timestamp lhs, Timestamp rhs)
    {
        return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
    }
    inline bool operator==(Timestamp lhs, Timestamp rhs)
    {
        return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
    }
    // 得到一个偏移时间
    inline Timestamp addTime(Timestamp timestamp, double seconds)
    {
        int64_t detal = seconds * Timestamp::kMicroSecondsPerSecond;
        return Timestamp(timestamp.microSecondsSinceEpoch() + detal);
    }

} // namespace net
