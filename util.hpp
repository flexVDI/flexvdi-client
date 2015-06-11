/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _UTIL_HPP_
#define _UTIL_HPP_

#include <system_error>
#include <ostream>
#include <functional>
#include <wchar.h>
#include <boost/preprocessor/cat.hpp>

namespace flexvm {

std::system_error lastSystemError(const std::string & msg);
#define throw_if(cond, msg) \
do { if (cond) throw lastSystemError(msg); } while(0)
#define return_if(cond, msg, ret) \
do { if (cond) { \
    LogError() << msg; \
    return ret; }} while(0)

struct on_return_struct {
    std::function<void(void)> f;
    template <typename T> on_return_struct(T c) : f(c) {}
    ~on_return_struct() { f(); }
};
#define on_return(code) \
on_return_struct BOOST_PP_CAT(on_return_, __LINE__)(code)

std::string toString(const std::wstring & str);
std::wstring toWstring(const std::string & str);

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
            prefix(*this, level);
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
        if (enabled)
            *out << toString(par);
        return *this;
    }
    Log & operator<<(wchar_t * par) {
        if (enabled)
            *out << toString(par);
        return *this;
    }
    Log & operator<<(const std::wstring & par) {
        if (enabled)
            *out << toString(par);
        return *this;
    }

    struct date {};
    Log & operator<<(const date & par) {
        logDate();
        return *this;
    }

    static void setLogOstream(std::ostream * logout) { out = logout; }
    static const char * getDefaultLogPath();
    template <typename P> static void setPrefix(P p) { prefix = p; }

protected:
    const bool enabled;
    #ifdef NDEBUG
    static const LogLevel minLevel = L_INFO;
    #else
    static const LogLevel minLevel = L_DEBUG;
    #endif
    static const char * levelStr[L_MAX_LEVEL];
    static std::ostream * out;
    static std::function<void(Log &, LogLevel)> prefix;
    static void logDate();
    static void defaultPrefix(Log & log, LogLevel level) {
        log << date() << levelStr[level];
    };
};


class LogError : public Log {
public:
    LogError() : Log(L_ERROR) {}
    ~LogError() {
        if (enabled) {
            std::system_error error = lastSystemError("");
            *out << ", " << error.code() << error.what();
        }
    }
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


std::string getTempFileName(const char * prefix);
std::string getTempDirName();

}

#endif // _UTIL_HPP_
