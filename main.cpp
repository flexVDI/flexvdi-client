/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include "FlexVDIGuestAgent.hpp"
#include "util.hpp"
using namespace flexvm;
using std::string;

int main(int argc, char * argv[]) {
    const char * logFileName = "/var/log/flexvdi_agent.log";
    std::ofstream logFile(logFileName, std::ios_base::app);
    logFile << std::endl << std::endl;
    Log::setLogOstream(&logFile);
    chmod(logFileName, S_IRUSR | S_IWUSR);
    Log(L_INFO) << "Starting the flexVDI guest agent.";

    FlexVDIGuestAgent & agent = FlexVDIGuestAgent::singleton();
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
