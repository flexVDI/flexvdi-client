/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include <fstream>
using std::cin;
#include "iapi.h"
#include "../../util.hpp"
using namespace flexvm;


static int GSDLLCALL gsInputCB(void *instance, char *buf, int len) {
    // Read from cin len chars or until a newline
    int count = 0;
    while (cin && count < len) {
        cin.get(buf[count]);
        if (buf[count++] == '\n') break;
    }
    return count;
}


static int GSDLLCALL gsOutputCB(void *instance, const char *str, int len) {
    // Ignore it
    return len;
}


static int GSDLLCALL gsErrorCB(void *instance, const char *str, int len) {
    Log(L_ERROR) << std::string(str, len);
    return len;
}


int main(int argc, char * argv[]) {
    /// Command line options used by GhostScript
    const char * gsArgv[] = {
        "PS2PDF",
        "-dNOPAUSE",
        "-dBATCH",
        "-dSAFER",
        "-sDEVICE=pdfwrite",
//         "-sOutputFile=\\\\.\\pipe\\flexvdi_print",
        "-sOutputFile=c:\\test.pdf",
        "-I.\\",
        "-c",
        ".setpdfwrite",
        "-"
    };

    std::ofstream logFile;
    logFile.open(Log::getDefaultLogPath() + std::string("\\flexvdi_print_filter.log"),
                 std::ios_base::app);
    logFile << std::endl << std::endl;
    Log::setLogOstream(&logFile);

    // Add the include directories to the command line flags we'll use with GhostScript:
//     if (::GetModuleFileName(NULL, cPath, MAX_PATH)) {
//         // Should be next to the application
//         char* pPos = strrchr(cPath, '\\');
//         if (pPos != NULL)
//             *(pPos) = '\0';
//         else
//             cPath[0] = '\0';
//         // OK, add the fonts and lib folders:
//         sprintf_s (cInclude, sizeof(cInclude), "-I%s\\urwfonts;%s\\lib", cPath, cPath);
//         gsArgv[6] = cInclude;
//     }

    // TODO: Read the first line with job options

    void * gsInstance;
    if (gsapi_new_instance(&gsInstance, NULL) < 0) {
        Log(L_ERROR) << "Could not create a GhostScript instance.";
        return 1;
    }

    if (gsapi_set_stdio(gsInstance, gsInputCB, gsOutputCB, gsErrorCB) < 0) {
        Log(L_ERROR) << "Could not set GhostScript callbacks.";
        gsapi_delete_instance(gsInstance);
        return 2;
    }

    // Run the GhostScript engine
    int result = gsapi_init_with_args(gsInstance, sizeof(gsArgv)/sizeof(char*), (char**)gsArgv);
    gsapi_exit(gsInstance);
    gsapi_delete_instance(gsInstance);

    return result;
}
