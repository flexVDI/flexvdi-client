/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _UTIL_HPP_
#define _UTIL_HPP_

#include <system_error>
#include <cerrno>
#include <ostream>

#define throw_if(cond, msg) \
    do { if (cond) throw std::system_error(errno, std::system_category(), msg); } while(0)

namespace flexvm {

enum LogLevel {
    DEBUG = 0,
    INFO,
    WARNING,
    ERROR
};

class Log {
public:
    Log(LogLevel level);
    virtual ~Log();
    template <typename T>
    Log & operator<<(const T & par) {
        *out << par;
        return *this;
    }
    
private:
    std::ostream * out;
};

}

#endif // _UTIL_HPP_
