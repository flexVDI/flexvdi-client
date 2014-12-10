/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include "FlexVMAgent.hpp"
using namespace flexvm;
using namespace asio;

int FlexVMAgent::run() {
    try {
        port.open();
        io.run();
    } catch (std::exception & error) {
        std::cerr << "[ERROR] " << error.what() << std::endl;
        return 1;
    }
    return 0;
}

void FlexVMAgent::parseOptions(int argc, char * argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::string("-e") == argv[i] && ++i < argc) {
            port.setEndpoint(argv[i]);
        }
    }
}
