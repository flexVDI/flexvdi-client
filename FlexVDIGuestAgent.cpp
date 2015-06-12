/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "FlexVDIGuestAgent.hpp"
#include "FlexVDIComponent.hpp"
#include "util.hpp"
using namespace flexvm;

int FlexVDIGuestAgent::run() {
    try {
        auto components = FlexVDIComponentFactory::singleton().instanceComponents();
        for (auto & comp : components)
            comp->registerComponent(dregistry);
        port.open();
        pipe.open();
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
