/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include <fstream>
#include <memory>
#include <algorithm>
#include <windows.h>
#include "iapi.h"
#include "../../util.hpp"
#include "../../FlexVDIProto.h"
#include "../send-job.hpp"
using namespace flexvm;
using namespace std;


static int GSDLLCALL gsOutputCB(void *instance, const char *str, int len) {
    Log(L_DEBUG) << string(str, len);
    return len;
}


static int GSDLLCALL gsErrorCB(void *instance, const char *str, int len) {
    Log(L_ERROR) << string(str, len);
    return len;
}


static string getTempFileName() {
    char tempPath[MAX_PATH] = ".\\";
    GetTempPathA(MAX_PATH, tempPath);
    char tempFilePath[MAX_PATH];
    GetTempFileNameA(tempPath, "fpj", 0, tempFilePath); // fpj = FlexVDI Print Job
    return string(tempFilePath);
}


int main(int argc, char * argv[]) {
    ofstream logFile;
    logFile.open(Log::getDefaultLogPath() + string("\\flexvdi_print_filter.log"),
                 ios_base::app);
    logFile << endl << endl;
    Log::setLogOstream(&logFile);

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

    string options;
    if (argc >= 2) options = argv[1];

    string fileName = getTempFileName();
    Log(L_DEBUG) << "Saving output to " << fileName;
    string fileOption = "-sOutputFile=" + fileName;
    gsArgv[5] = fileOption.c_str();

    void * gsInstance;
    if (gsapi_new_instance(&gsInstance, NULL) < 0) {
        Log(L_ERROR) << "Could not create a GhostScript instance.";
        return 1;
    }
    Log(L_DEBUG) << "Instance created";

    if (gsapi_set_stdio(gsInstance, NULL, gsOutputCB, gsErrorCB) < 0) {
        Log(L_ERROR) << "Could not set GhostScript callbacks.";
        gsapi_delete_instance(gsInstance);
        return 2;
    }
    Log(L_DEBUG) << "Callbacks set";

    // Run the GhostScript engine
    int result = gsapi_init_with_args(gsInstance, sizeof(gsArgv)/sizeof(char*), (char**)gsArgv);
    gsapi_exit(gsInstance);
    gsapi_delete_instance(gsInstance);
    Log(L_DEBUG) << "GS returned " << result;

    ifstream pdfFile(fileName.c_str(), ios::binary);
    if (pdfFile) {
        Log(L_DEBUG) << "PDF file correctly created, sending to guest agent";
        result = sendJob(pdfFile, options) ? 0 : 1;
        pdfFile.close();
        DeleteFileA(fileName.c_str());
    }

    return result;
}
