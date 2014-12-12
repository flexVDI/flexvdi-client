/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "FlexVMAgent.hpp"
#include "FlexVMProto.h"
#include "util.hpp"
using namespace flexvm;

int main(int argc, char * argv[]) {
    registerMessageMarshallers();
    FlexVMAgent agent(argc, argv);
    return agent.run();
}

static const char * levelStr[] = { "DEBUG", "INFO", "WARNING", "ERROR" };

Log::Log(LogLevel level) {
    out = &std::cerr;
    std::cerr << "[" << levelStr[level] << "] ";
}

Log::~Log() {
    std::cerr << std::endl;
}
