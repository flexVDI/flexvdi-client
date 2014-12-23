/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include <iomanip>
#include <ctime>
#include <chrono>
#define FLEXVDI_PROTO_IMPL
#include "FlexVDIProto.h"
#include "util.hpp"
using namespace flexvm;
using std::string;
using namespace std::chrono;


std::ostream * Log::out = &std::cerr;
const char * Log::levelStr[L_MAX_LEVEL] = { "DEBUG", "INFO", "WARNING", "ERROR" };


void Log::logDate() {
    static char date[20];
    auto now = high_resolution_clock::now();
    microseconds us = duration_cast<microseconds>(now.time_since_epoch());
    std::time_t now_sec = duration_cast<seconds>(us).count();
    std::size_t fraction = us.count() % 1000000;
    std::strftime(date, 20, "%Y/%m/%d %H:%M:%S", std::localtime(&now_sec));
    *out << date << '.' << std::setfill('0') << std::setw(6) << fraction;
}
