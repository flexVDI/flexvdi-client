/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifdef WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <cerrno>
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <memory>
#include <random>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <chrono>
#define FLEXVDI_PROTO_IMPL
#include "FlexVDIProto.h"
#include "util.hpp"
using namespace flexvm;
using namespace std::chrono;


std::ostream * Log::out = &std::cerr;
const char * Log::levelStr[L_MAX_LEVEL] = { "DEBUG", "INFO", "WARNING", "ERROR" };
std::function<void(Log &, LogLevel)> Log::prefix = Log::defaultPrefix;


void Log::logDate() {
    static char date[20];
    auto now = high_resolution_clock::now();
    microseconds us = duration_cast<microseconds>(now.time_since_epoch());
    std::time_t now_sec = duration_cast<seconds>(us).count();
    std::size_t fraction = us.count() % 1000000;
    std::strftime(date, 20, "%d/%m/%Y %H:%M:%S", std::localtime(&now_sec));
    *out << date << '.' << std::setfill('0') << std::setw(6) << fraction;
}


#ifdef WIN32
static inline int utf8ToUtf16(const char * in, size_t length, wchar_t * out, size_t size) {
    return MultiByteToWideChar(CP_UTF8, 0, in, length, out, size);
}


static inline int utf16ToUtf8(const wchar_t * in, size_t length, char * out, size_t size) {
    return WideCharToMultiByte(CP_UTF8, 0, in, length, out, size, NULL, NULL);
}

#else

// TODO: Force UTF-8 locale
static inline int utf8ToUtf16(const char * in, size_t length, wchar_t * out, size_t size) {
    return mbstowcs(out, in, size);
}


static inline int utf16ToUtf8(const wchar_t * in, size_t length, char * out, size_t size) {
    return wcstombs(out, in, size);
}
#endif


std::string flexvm::toString(const std::wstring & str) {
    size_t size = str.length()*4 + 1; // The maximum size of an UTF8 encoding of str
    std::unique_ptr<char[]> buffer(new char[size]);
    int length = utf16ToUtf8(str.c_str(), str.length(), buffer.get(), size);
    return std::string(buffer.get(), length);
}


std::wstring flexvm::toWstring(const std::string & str) {
    size_t size = str.length() + 1;
    std::unique_ptr<wchar_t[]> buffer(new wchar_t[size]);
    int length = utf8ToUtf16(str.c_str(), str.length(), buffer.get(), size);
    return std::wstring(buffer.get(), length);
}


std::string flexvm::getTempFileName(const char * prefix) {
    static std::default_random_engine generator(
        high_resolution_clock::now().time_since_epoch().count()
    );
    const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::uniform_int_distribution<int> distribution(0, sizeof(charset) - 1);
    char randomString[9];
    for (int i = 0; i < 8; ++i)
        randomString[i] = charset[distribution(generator)];
    randomString[8] = '\0';
    return getTempDirName() + prefix + randomString;
}


#ifdef WIN32
const char * Log::getDefaultLogPath() {
    static char logPath[MAX_PATH + 9] = "c:";
    SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, logPath);
    size_t length = strlen(logPath);
    std::copy_n("\\flexVDI", 9, &logPath[length]);
    if (SHCreateDirectoryExA(NULL, logPath, NULL) || GetLastError() == ERROR_ALREADY_EXISTS)
        return logPath;
    else
        return "c:";
}


std::system_error flexvm::lastSystemError(const std::string & msg) {
    return std::system_error(GetLastError(), std::system_category(), msg);
}


std::string flexvm::getTempDirName() {
    char tempPath[MAX_PATH] = ".\\";
    GetTempPathA(MAX_PATH, tempPath);
    return std::string(tempPath);
}


#else
const char * Log::getDefaultLogPath() {
    if (!mkdir("/var/log/flexVDI", 0755) || errno == EEXIST)
        return "/var/log/flexVDI";
    else if (getenv("TMPDIR"))
        return getenv("TMPDIR");
    else
        return "/tmp";
}


std::system_error flexvm::lastSystemError(const std::string & msg) {
    return std::system_error(errno, std::system_category(), msg);
}


std::string flexvm::getTempDirName() {
    const char * envTmp = getenv("TMPDIR");
    if (envTmp) return std::string(envTmp) + "/";
    return std::string("/tmp/");
}

#endif
