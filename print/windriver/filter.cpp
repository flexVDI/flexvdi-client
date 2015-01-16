/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include <fstream>
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <windows.h>
using std::cin;
#include "iapi.h"
#include "../../util.hpp"
#include "../../FlexVDIProto.h"
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
    Log(L_DEBUG) << std::string(str, len);
    return len;
}


static int GSDLLCALL gsErrorCB(void *instance, const char *str, int len) {
    Log(L_ERROR) << std::string(str, len);
    return len;
}


static std::string getTempFileName() {
    char tempPath[MAX_PATH] = ".\\";
    GetTempPathA(MAX_PATH, tempPath);
    char tempFilePath[MAX_PATH];
    GetTempFileNameA(tempPath, "fpj", 0, tempFilePath); // fpj = FlexVDI Print Job
    return std::string(tempFilePath);
}


static void notifyJob(const std::string fileName) {
}


int main(int argc, char * argv[]) {
    /// Command line options used by GhostScript
    const char * gsArgv[] = {
        "PS2PDF",
        "-dNOPAUSE",
        "-dBATCH",
        "-dSAFER",
        "-sDEVICE=pdfwrite",
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

    std::string fileName = getTempFileName();
    Log(L_DEBUG) << "Saving output to " << fileName;
    std::string fileOption = "-sOutputFile=" + fileName;
    gsArgv[5] = fileOption.c_str();

    void * gsInstance;
    if (gsapi_new_instance(&gsInstance, NULL) < 0) {
        Log(L_ERROR) << "Could not create a GhostScript instance.";
        return 1;
    }
    Log(L_DEBUG) << "Instance created";

    if (gsapi_set_stdio(gsInstance, gsInputCB, gsOutputCB, gsErrorCB) < 0) {
        Log(L_ERROR) << "Could not set GhostScript callbacks.";
        gsapi_delete_instance(gsInstance);
        return 2;
    }
    Log(L_DEBUG) << "Callbacks set";

    // Run the GhostScript engine
    int result = gsapi_init_with_args(gsInstance, sizeof(gsArgv)/sizeof(char*), (char**)gsArgv);
    gsapi_exit(gsInstance);
    gsapi_delete_instance(gsInstance);

    notifyJob(fileName);

    return result;
}
