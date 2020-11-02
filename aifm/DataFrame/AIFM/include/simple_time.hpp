#pragma once

#include <cstdint>
#include <iostream>

struct SimpleTime {
    short year_;
    char month_;
    char day_;
    char hour_;
    char min_;
    char second_;

    SimpleTime() {}

    SimpleTime(short year, char month, char day, char hour, char min, char second)
        : year_(year), month_(month), day_(day), hour_(hour), min_(min), second_(second)
    {
    }

    uint64_t to_second() const
    {
        uint64_t ret = year_;
        ret          = 12 * ret + month_;
        // A simple approximation on days.
        ret = 30 * ret + day_;
        ret = 24 * ret + hour_;
        ret = 60 * ret + min_;
        ret = 60 * ret + second_;
        return ret;
    }

    friend std::istream& operator>>(std::istream& in, SimpleTime& t)
    {
        char delim;
        uint16_t month, day, hour, min, second;
        in >> t.year_ >> delim >> month >> delim >> day >> delim >> hour >> delim >> min >> delim >>
            second;
        t.month_  = month;
        t.day_    = day;
        t.hour_   = hour;
        t.min_    = min;
        t.second_ = second;
        return in;
    }

    friend std::ostream& operator<<(std::ostream& out, const SimpleTime& t)
    {
        out << t.year_ << '-' << (int)t.month_ << '-' << (int)t.day_ << ' ' << (int)t.hour_ << ':'
            << (int)t.min_ << ':' << (int)t.second_;
        return out;
    }

    bool operator<(const SimpleTime& o) const
    {
        if (year_ != o.year_) return year_ < o.year_;
        if (month_ != o.month_) return month_ < o.month_;
        if (day_ != o.day_) return day_ < o.day_;
        if (hour_ != o.hour_) return hour_ < o.hour_;
        if (min_ != o.min_) return min_ < o.min_;
        if (second_ != o.second_) return second_ < o.second_;
        return false;
    }

    bool operator>(const SimpleTime& o) const
    {
        if (year_ != o.year_) return year_ > o.year_;
        if (month_ != o.month_) return month_ > o.month_;
        if (day_ != o.day_) return day_ > o.day_;
        if (hour_ != o.hour_) return hour_ > o.hour_;
        if (min_ != o.min_) return min_ > o.min_;
        if (second_ != o.second_) return second_ > o.second_;
        return false;
    }

    bool operator==(const SimpleTime& o) const
    {
        return __builtin_strncmp(reinterpret_cast<const char*>(this),
                                 reinterpret_cast<const char*>(&o), sizeof(o)) == 0;
    }

    bool operator!=(const SimpleTime& o) const
    {
        return !operator==(o);
    }
};

template <> struct std::hash<SimpleTime> {
    size_t operator()(const SimpleTime& simple_time) const
    {
        uint64_t hash = 0;
        static_assert(sizeof(simple_time) <= sizeof(hash));
        __builtin_memcpy(&hash, &simple_time, sizeof(simple_time));
        return hash;
    }
};

