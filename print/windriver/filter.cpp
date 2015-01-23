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
using namespace flexvm;
using namespace std;


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


static void notifyJob(const string fileName) {
    // TODO: Configurable
    static const char * pipeName = "\\\\.\\pipe\\flexvdi_pipe";
    HANDLE pipe = ::CreateFileA(pipeName, GENERIC_READ | GENERIC_WRITE,
                                0, NULL, OPEN_EXISTING, 0, NULL);
    return_if(pipe == INVALID_HANDLE_VALUE, "Failed opening pipe", );
    size_t optionsLength = fileName.length() + 9; // 9 = length of "filename="
    size_t bufSize = sizeof(FlexVDIMessageHeader) + sizeof(FlexVDIPrintJobMsg) + optionsLength;
    unique_ptr<uint8_t[]> buffer(new uint8_t[bufSize]);
    FlexVDIMessageHeader * header = (FlexVDIMessageHeader *)buffer.get();
    header->type = FLEXVDI_PRINTJOB;
    header->size = bufSize - sizeof(FlexVDIMessageHeader);
    FlexVDIPrintJobMsg * msg = (FlexVDIPrintJobMsg *)(header + 1);
    msg->optionsLength = optionsLength;
    copy_n("filename=", 9, msg->options);
    copy_n(fileName.c_str(), fileName.length(), &msg->options[9]);
    DWORD bytesWritten;
    if (!::WriteFile(pipe, buffer.get(), bufSize, &bytesWritten, NULL)
        || bytesWritten < bufSize) {
        Log(L_ERROR) << "Error notifying print job" << lastSystemError("").what();
    }
    ReadFile(pipe, buffer.get(), 1, &bytesWritten, NULL);
    // When the connection is closed, the pdf file can be removed
    DeleteFileA(fileName.c_str());
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

    // TODO: Read the first line with job options

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
