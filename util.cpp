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
#include <boost/locale.hpp>
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


std::string flexvm::toString(const std::wstring & str) {
    return boost::locale::conv::utf_to_utf<char>(str);
}


std::wstring flexvm::toWstring(const std::string & str) {
    return boost::locale::conv::utf_to_utf<wchar_t>(str);
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
