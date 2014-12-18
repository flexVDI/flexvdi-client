/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "FlexVDIGuestAgent.hpp"
#include "util.hpp"
using namespace flexvm;

int FlexVDIGuestAgent::run() {
    try {
        registerHandlers();
        port.open();
        creds.open();
        io.run();
    } catch (std::exception & error) {
        Log(L_ERROR) << error.what();
        return 1;
    }
    return 0;
}


void FlexVDIGuestAgent::stop() {
    io.stop();
}


void FlexVDIGuestAgent::registerHandlers() {
    creds.registerHandlers(dregistry);
}


void FlexVDIGuestAgent::handle(uint32_t type, const std::shared_ptr<uint8_t> & msgBuffer) {
    if (!dregistry.dispatch(type, msgBuffer)) {
        Log(L_WARNING) << "Undispatched message type " << type;
    }
}
