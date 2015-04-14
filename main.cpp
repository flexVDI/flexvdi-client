/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include <fstream>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include "FlexVDIGuestAgent.hpp"
#include "util.hpp"
using namespace flexvm;
using std::string;


static void removePidFile(int signum) {
    Log(L_INFO) << "Finishing flexVDI guest agent.";
    unlink("/var/run/flexvdi-guest-agent.pid");
    exit(0);
}


int main(int argc, char * argv[]) {
    bool doFork = true;
    char * endpoint = NULL;
    for (int i = 1; i < argc; ++i) {
        if (string("-e") == argv[i] && ++i < argc) {
            endpoint = argv[i];
        } else if (string("-x") == argv[i]) {
            doFork = false;
        } else {
            std::cout << "Usage: " << argv[0] << " [ -e endpoint ] [-x]" << std::endl;
            return 1;
        }
    }

    pid_t pid = 0;
    if (doFork) {
        pid = fork();
        if (pid > 0) {
            std::ofstream pidFile("/var/run/flexvdi-guest-agent.pid");
            pidFile << pid << std::endl;
            return 0;
        } else if (pid < 0) {
            std::cerr << "Error starting background process" << std::endl;
            return 2;
        }

        pid_t sid = setsid();
        if (sid < 0) {
            Log(L_ERROR) << "Failed to create a new session ID";
            exit(EXIT_FAILURE);
        }
        if ((chdir("/")) < 0) {
            exit(EXIT_FAILURE);
        }
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    const char * logFileName = "/var/log/flexvdi_agent.log";
    std::ofstream logFile(logFileName, std::ios_base::app);
    logFile << std::endl << std::endl;
    Log::setLogOstream(&logFile);
    chmod(logFileName, S_IRUSR | S_IWUSR);
    Log(L_INFO) << "Starting the flexVDI guest agent.";

    signal(SIGINT, removePidFile);
    signal(SIGTERM, removePidFile);

    FlexVDIGuestAgent & agent = FlexVDIGuestAgent::singleton();
    if (endpoint) {
        Log(L_DEBUG) << "Using endpoint " << endpoint;
        agent.setVirtioEndpoint(endpoint);
    }
    return agent.run();
}
