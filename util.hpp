/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _UTIL_HPP_
#define _UTIL_HPP_

#include <system_error>
#include <ostream>

#define throw_code_if(cond, code, msg) \
do { if (cond) throw std::system_error(code, std::system_category(), msg); } while(0)
#define return_code_if(cond, code, msg, ret) \
do { if (cond) { \
    Log(L_ERROR) << msg << ": " << std::system_category().message(code); return ret; \
}} while(0)

#ifdef WIN32
#define throw_if(cond, msg) throw_code_if(cond, GetLastError(), msg)
#define return_if(cond, msg, ret) return_code_if(cond, GetLastError(), msg, ret)
#else
#include <cerrno>
#define throw_if(cond, msg) throw_code_if(cond, errno, msg)
#define return_if(cond, msg, ret) return_code_if(cond, errno, msg, ret)
#endif

namespace flexvm {

enum LogLevel {
    L_DEBUG = 0,
    L_INFO,
    L_WARNING,
    L_ERROR
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

    static void setLogOstream(std::ostream * logout) { out = logout; }

private:
    static std::ostream * out;
};

}

#endif // _UTIL_HPP_
