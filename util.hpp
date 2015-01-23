/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _UTIL_HPP_
#define _UTIL_HPP_

#include <system_error>
#include <ostream>
#include <functional>
#include <wchar.h>

namespace flexvm {

std::system_error lastSystemError(const std::string & msg);
#define throw_if(cond, msg) \
do { if (cond) throw lastSystemError(msg); } while(0)
#define return_if(cond, msg, ret) \
do { if (cond) { Log(L_ERROR) << msg << lastSystemError("").what(); return ret; }} while(0)

struct on_return {
    std::function<void(void)> f;
    template <typename T> on_return(T c) : f(c) {}
    ~on_return() { f(); }
};

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
    Log & operator<<(const wchar_t * par) {
        if (enabled) {
            char buffer[512];
            wcstombs(buffer, par, 511);
            buffer[511] = '\0';
            *out << buffer;
        }
        return *this;
    }
    Log & operator<<(wchar_t * par) {
        return operator<<(const_cast<const wchar_t *>(par));
    }

    static void setLogOstream(std::ostream * logout) { out = logout; }
    static const char * getDefaultLogPath();

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
