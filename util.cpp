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
#include <ctime>
#include <cstring>
#include <chrono>
#define FLEXVDI_PROTO_IMPL
#include "FlexVDIProto.h"
#include "util.hpp"
using namespace flexvm;
using namespace std::chrono;


std::ostream * Log::out = &std::cerr;
const char * Log::levelStr[L_MAX_LEVEL] = { "DEBUG", "INFO", "WARNING", "ERROR" };


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


#else
const char * Log::getDefaultLogPath() {
    if (!mkdir("/var/log/flexVDI", 0755) || errno == EEXIST)
        return "/var/log/flexVDI";
    else
        return "/tmp";
}


std::system_error flexvm::lastSystemError(const std::string & msg) {
    return std::system_error(errno, std::system_category(), msg);
}
#endif
