/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "FlexVMAgent.hpp"
#include "util.hpp"
using namespace flexvm;

int FlexVMAgent::run() {
    try {
        registerHandlers();
        port.open();
        io.run();
    } catch (std::exception & error) {
        Log(ERROR) << error.what();
        return 1;
    }
    return 0;
}

void FlexVMAgent::registerHandlers() {
    creds.registerHandlers(dregistry);
}

void FlexVMAgent::parseOptions(int argc, char * argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::string("-e") == argv[i] && ++i < argc) {
            Log(DEBUG) << "Using endpoint " << argv[i];
            port.setEndpoint(argv[i]);
        }
    }
}

void FlexVMAgent::handle(uint32_t type, const std::shared_ptr<uint8_t> & msgBuffer) {
    if (!dregistry.dispatch(type, msgBuffer)) {
        Log(WARNING) << "Undispatched message type " << type;
    }
}
