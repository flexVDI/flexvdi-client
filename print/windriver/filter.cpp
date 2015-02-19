/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <algorithm>
#include <windows.h>
#include <winspool.h>
#include "iapi.h"
#include "../../util.hpp"
#include "../../FlexVDIProto.h"
#include "../send-job.hpp"
using namespace flexvm;
using namespace std;


static int GSDLLCALL gsOutputCB(void *instance, const char *str, int len) {
    static string line;
    line += string(str, len);
    size_t newline;
    while ((newline = line.find_first_of('\n')) != string::npos) {
        Log(L_DEBUG) << line.substr(0, newline);
        line = line.substr(newline + 1);
    }
    return len;
}


static int GSDLLCALL gsErrorCB(void *instance, const char *str, int len) {
    static string line;
    line += string(str, len);
    size_t newline;
    while ((newline = line.find_first_of('\n')) != string::npos) {
        Log(L_ERROR) << line.substr(0, newline);
        line = line.substr(newline + 1);
    }
    return len;
}


static string getTempFileName() {
    char tempPath[MAX_PATH] = ".\\";
    GetTempPathA(MAX_PATH, tempPath);
    char tempFilePath[MAX_PATH];
    GetTempFileNameA(tempPath, "fpj", 0, tempFilePath); // fpj = FlexVDI Print Job
    return string(tempFilePath);
}


shared_ptr<JOB_INFO_2A> getJobInfo() {
    wchar_t printerName[512];
    GetEnvironmentVariableW(L"REDMON_PRINTER", printerName, 512);
    wchar_t jobIdString[10];
    GetEnvironmentVariableW(L"REDMON_JOB", jobIdString, 10);

    int jobId = stoi(jobIdString);
    HANDLE printer;
    return_if(!OpenPrinterW(printerName, &printer, NULL), "Failed opening printer",
              shared_ptr<JOB_INFO_2A>());

    DWORD needed;
    GetJobA(printer, jobId, 2, NULL, 0, &needed);
    std::shared_ptr<char> buffer(new char[needed], std::default_delete<char[]>());
    return_if(!GetJobA(printer, jobId, 2, (LPBYTE)buffer.get(), needed, &needed),
              "Failed getting document properties", shared_ptr<JOB_INFO_2A>());
    return shared_ptr<JOB_INFO_2A>(buffer, (JOB_INFO_2A *)buffer.get());
}


string getMedia(DEVMODEA * devMode) {
    ostringstream media;
    switch (devMode->dmPaperSize) {
        case DMPAPER_TABLOID: media << "Tabloid"; break;
        case DMPAPER_LEDGER: media << "Ledger"; break;
        case DMPAPER_LEGAL: media << "Legal"; break;
        case DMPAPER_LETTER: media << "Letter"; break;
        case DMPAPER_LETTERSMALL: media << "LetterSmall"; break;
        case DMPAPER_STATEMENT: media << "Statement"; break;
        case DMPAPER_NOTE: media << "Note"; break;
//         case DMPAPER_A0: media << "A0"; break;
//         case DMPAPER_A1: media << "A1"; break;
        case DMPAPER_A2: media << "A2"; break;
        case DMPAPER_A3: media << "A3"; break;
        case DMPAPER_A4: media << "A4"; break;
        case DMPAPER_A4SMALL: media << "A4Small"; break;
        case DMPAPER_A5: media << "A5"; break;
        case DMPAPER_A6: media << "A6"; break;
//         case DMPAPER_A7: media << "A7"; break;
//         case DMPAPER_A8: media << "A8"; break;
//         case DMPAPER_A9: media << "A9"; break;
//         case DMPAPER_A10: media << "A10"; break;
        case DMPAPER_B4: media << "B4"; break;
        case DMPAPER_B5: media << "B5"; break;
//         case DMPAPER_B6: media << "B6"; break;
        case DMPAPER_ENV_C3: media << "EnvC3"; break;
        case DMPAPER_ENV_C4: media << "EnvC4"; break;
        case DMPAPER_ENV_C5: media << "EnvC5"; break;
        case DMPAPER_ENV_C6: media << "EnvC6"; break;
        default: media << devMode->dmPaperWidth << 'x' << devMode->dmPaperLength;
    }
    if (devMode->dmMediaType == DMMEDIA_TRANSPARENCY) {
        media << ",Transparency";
    } else if (devMode->dmMediaType == DMMEDIA_GLOSSY) {
        media << ",Glossy";
    }
    // TODO: Add media source
    return media.str();
}


string getJobOptions(DEVMODEA * devMode) {
    char jobName[512];
    GetEnvironmentVariableA("REDMON_DOCNAME", jobName, 512);

    ostringstream oss;
    oss << "title=\"" << jobName << "\"";
    oss << " media=" << getMedia(devMode);
    switch (devMode->dmDuplex) {
        case DMDUP_HORIZONTAL: oss << " sides=two-sided-short-edge"; break;
        case DMDUP_VERTICAL: oss << " sides=two-sided-long-edge"; break;
        default: oss << " sides=one-sided"; break;
    }
    oss << " copies=" << devMode->dmCopies;
    oss << (devMode->dmCollate == DMCOLLATE_TRUE ? " Collate" : " noCollate");
    int resolution = devMode->dmPrintQuality;
    if (resolution < 0) resolution = devMode->dmYResolution;
    oss << " Resolution=" << resolution << "dpi";
    oss << (devMode->dmColor == DMCOLOR_COLOR ? " color" : " bn");

    return oss.str();
}


void dumpStdin(const string & fileName) {
    ofstream(fileName.c_str(), ios::binary) << cin.rdbuf();
}


int runGhostscript(int argc, char * argv[]) {
    Log(L_DEBUG) << "Running GhostScript";
    void * gsInstance;
    if (gsapi_new_instance(&gsInstance, NULL) < 0) {
        Log(L_ERROR) << "Could not create a GhostScript instance.";
        return 1;
    }

    if (gsapi_set_stdio(gsInstance, NULL, gsOutputCB, gsErrorCB) < 0) {
        Log(L_ERROR) << "Could not set GhostScript callbacks.";
        gsapi_delete_instance(gsInstance);
        return 2;
    }

    // Run the GhostScript engine
    int result = gsapi_init_with_args(gsInstance, argc, argv);
    gsapi_exit(gsInstance);
    gsapi_delete_instance(gsInstance);
    Log(L_DEBUG) << "GhostScript returned " << result;
    return result;
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
        "-",
        ""
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

    string psFileName = getTempFileName();
    dumpStdin(psFileName);
    auto jobInfo = getJobInfo();
    int numPages = jobInfo->PagesPrinted;

    string inFileOption = "-f" + psFileName;
    gsArgv[9] = inFileOption.c_str();
    string pdfFileName = getTempFileName();
    Log(L_DEBUG) << "Saving output to " << pdfFileName;
    string outFileOption = "-sOutputFile=" + pdfFileName;
    gsArgv[5] = outFileOption.c_str();
    int result = runGhostscript(10, (char **)gsArgv);

    if (jobInfo->pDevMode->dmCollate == DMCOLLATE_TRUE && jobInfo->pDevMode->dmCopies > 1) {
        // The spooler performed the copies, keep just one
        numPages /= jobInfo->pDevMode->dmCopies;
        string lastPageOption = "-dLastPage=" + to_string(numPages);
        inFileOption = "-f" + pdfFileName;
        pdfFileName = getTempFileName();
        outFileOption = "-sOutputFile=" + pdfFileName;
        gsArgv[5] = outFileOption.c_str();
        gsArgv[9] = lastPageOption.c_str();
        gsArgv[10] = inFileOption.c_str();
        result = runGhostscript(11, (char **)gsArgv);
    }

    ifstream pdfFile(pdfFileName.c_str(), ios::binary);
    if (result == 0 && pdfFile) {
        Log(L_DEBUG) << "PDF file correctly created, sending to guest agent";
        result = sendJob(pdfFile, getJobOptions(jobInfo->pDevMode)) ? 0 : 1;
        pdfFile.close();
        DeleteFileA(pdfFileName.c_str());
    }

    return result;
}
