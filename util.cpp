/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include <ctime>
#define FLEXVDI_PROTO_IMPL
#include "FlexVDIProto.h"
#include "util.hpp"
using namespace flexvm;
using std::string;

std::ostream * Log::out = &std::cerr;
static const char * levelStr[] = { "DEBUG", "INFO", "WARNING", "ERROR" };


Log::Log(LogLevel level) {
    time_t now = std::time(nullptr);
    char date[20];
    std::strftime(date, 20, "%Y/%m/%d %H:%M:%S", std::localtime(&now));
    *out << date << " [" << levelStr[level] << "] ";
}


Log::~Log() {
    *out << std::endl;
}
