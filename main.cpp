/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include "FlexVDIGuestAgent.hpp"
#include "util.hpp"
using namespace flexvm;
using std::string;

int main(int argc, char * argv[]) {
    FlexVDIGuestAgent agent;
    for (int i = 1; i < argc; ++i) {
        if (string("-e") == argv[i] && ++i < argc) {
            Log(L_DEBUG) << "Using endpoint " << argv[i];
            agent.setVirtioEndpoint(argv[i]);
        } else {
            std::cout << "Usage: " << argv[0] << " [ -e endpoint ]" << std::endl;
            return 1;
        }
    }

    return agent.run();
}
