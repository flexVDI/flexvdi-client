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
    L_ERROR,
    L_MAX_LEVEL
};

class Log {
public:
    Log(LogLevel level) : enabled(level >= minLevel) {
        if (enabled) {
            logDate();
            *out << " [" << levelStr[level] << "] ";
        }
    }
    virtual ~Log() {
        if (enabled) {
            *out << std::endl;
            out->flush();
        }
    }
    template <typename T>
    Log & operator<<(const T & par) {
        if (enabled)
            *out << par;
        return *this;
    }

    static void setLogOstream(std::ostream * logout) { out = logout; }

private:
    bool enabled;
    #ifdef NDEBUG
    static const LogLevel minLevel = L_INFO;
    #else
    static const LogLevel minLevel = L_DEBUG;
    #endif
    static const char * levelStr[L_MAX_LEVEL];
    static std::ostream * out;
    static void logDate();
};


struct LogCall {
    std::string fn;
    LogCall(const char * funcName) : fn(funcName) {
        Log(L_DEBUG) << "-->" << fn;
    }
    ~LogCall() {
        Log(L_DEBUG) << "<--" << fn;
    }
};


}

#endif // _UTIL_HPP_
