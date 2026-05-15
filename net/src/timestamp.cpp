#include "timestamp.h"
#include <sys/time.h>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace net
{
    Timestamp::Timestamp() : _microSecondsSinceEpoch(0) {}
    Timestamp::Timestamp(int64_t usec) : _microSecondsSinceEpoch(usec) {}
    bool Timestamp::valid() const { return _microSecondsSinceEpoch > 0; }
    void Timestamp::swap(Timestamp &other) { std::swap(_microSecondsSinceEpoch, other._microSecondsSinceEpoch); }
    int64_t Timestamp::microSecondsSinceEpoch() const { return _microSecondsSinceEpoch; }
    time_t Timestamp::secondsSinceEpoch() const { return _microSecondsSinceEpoch / kMicroSecondsPerSecond; }
    // 将时间戳转换并返回⼀个时间戳对象
    Timestamp Timestamp::fromUnixTime(time_t t) { return fromUnixTime(t, 0); }
    Timestamp Timestamp::fromUnixTime(time_t t, int microseconds) { return Timestamp(t * kMicroSecondsPerSecond + microseconds); }

    // 转换为 yyyy-mm-dd hh:mm:ss gmtime_r
    std::string Timestamp::toFormattedString()
    {
        const time_t timeval = secondsSinceEpoch();
        struct tm time;
        localtime_r(&timeval, &time);
        char buff[32] = {};
        strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", &time);
        return std::string(buff);
    }

    Timestamp Timestamp::now()
    {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        int64_t totalmico = tv.tv_sec * kMicroSecondsPerSecond + tv.tv_usec;
        return Timestamp(totalmico);
    }

    Timestamp Timestamp::invalid() { return Timestamp{}; }

} // namespace net
