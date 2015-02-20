/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <algorithm>
#include <vector>
#include <windows.h>
#include <winspool.h>
#include "iapi.h"
#include "../../util.hpp"
#include "../../FlexVDIProto.h"
#include "../send-job.hpp"
using namespace flexvm;
using namespace std;


class GSFilter {
public:
    static GSFilter & singleton() {
        static GSFilter instance;
        return instance;
    }

    static int GSDLLCALL gsOutputCB(void * instance, const char * str, int len) {
        singleton().out(str, len);
        return len;
    }

    static int GSDLLCALL gsErrorCB(void * instance, const char * str, int len) {
        singleton().err(str, len);
        return len;
    }

    int runFilter();

private:
    string outBuffer, errBuffer;
    vector<string> tmpFiles;
    string inFileName, outFileName, extraOptions;
    string printerName, docName;
    int jobId;
    shared_ptr<JOB_INFO_2A> jobInfo;

    GSFilter();

    void flush(string & line, LogLevel level) {
        size_t newline;
        while ((newline = line.find_first_of('\n')) != string::npos) {
            Log(level) << line.substr(0, newline);
            line = line.substr(newline + 1);
        }
    }

    void out(const char * str, int len) {
        flush(outBuffer += string(str, len), L_INFO);
    }

    void err(const char * str, int len) {
        flush(errBuffer += string(str, len), L_ERROR);
    }

    string getTempFileName() {
        char tempPath[MAX_PATH] = ".\\";
        GetTempPathA(MAX_PATH, tempPath);
        char tempFilePath[MAX_PATH];
        GetTempFileNameA(tempPath, "fpj", 0, tempFilePath); // fpj = FlexVDI Print Job
        tmpFiles.push_back(string(tempFilePath));
        return tmpFiles.back();
    }

    void readEnv() {
        char buffer[512];
        GetEnvironmentVariableA("REDMON_PRINTER", buffer, 512);
        printerName = buffer;
        GetEnvironmentVariableA("REDMON_JOB", buffer, 512);
        jobId = stoi(buffer);
        GetEnvironmentVariableA("REDMON_DOCNAME", buffer, 512);
        docName = buffer;
    }

    void getJobInfo();
    string getMedia();
    string getJobOptions();

    void dumpStdin(const string & fileName) {
        ofstream(fileName.c_str(), ios::binary) << cin.rdbuf();
    }

    int runGhostscript();

};


int main(int argc, char * argv[]) {
    ofstream logFile;
    logFile.open(Log::getDefaultLogPath() + string("\\flexvdi_print_filter.log"),
                 ios_base::app);
    logFile << endl << endl;
    Log::setLogOstream(&logFile);
    return GSFilter::singleton().runFilter();
}


GSFilter::GSFilter() {
    readEnv();
    getJobInfo();
    inFileName = getTempFileName();
    outFileName = getTempFileName();
}


int GSFilter::runFilter() {
    dumpStdin(inFileName);
    Log(L_DEBUG) << "Saving output to " << outFileName;
    int result = runGhostscript();

    if (jobInfo->pDevMode->dmCollate == DMCOLLATE_TRUE && jobInfo->pDevMode->dmCopies > 1) {
        // Update the number of pages printed
        getJobInfo();
        // The spooler performed the copies, keep just one
        int numPages = jobInfo->PagesPrinted / jobInfo->pDevMode->dmCopies;
        extraOptions = "-dLastPage=" + to_string(numPages);
        inFileName = outFileName;
        outFileName = getTempFileName();
        result = runGhostscript();
    }

    ifstream pdfFile(outFileName.c_str(), ios::binary);
    if (result == 0 && pdfFile) {
        Log(L_DEBUG) << "PDF file correctly created, sending to guest agent";
        result = sendJob(pdfFile, getJobOptions()) ? 0 : 1;
        pdfFile.close();
    }

    for (auto & f : tmpFiles)
        DeleteFileA(f.c_str());

    return result;
}


void GSFilter::getJobInfo() {
    HANDLE printer;
    if (OpenPrinterA((char *)printerName.c_str(), &printer, NULL)) {
        DWORD needed;
        GetJobA(printer, jobId, 2, NULL, 0, &needed);
        std::shared_ptr<char> buffer(new char[needed], std::default_delete<char[]>());
        if(GetJobA(printer, jobId, 2, (LPBYTE)buffer.get(), needed, &needed)) {
            jobInfo = shared_ptr<JOB_INFO_2A>(buffer, (JOB_INFO_2A *)buffer.get());
        }
        ClosePrinter(printer);
    }
}


string GSFilter::getMedia() {
    DEVMODEA * devMode = jobInfo->pDevMode;
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


string GSFilter::getJobOptions() {
    DEVMODEA * devMode = jobInfo->pDevMode;
    ostringstream oss;
    oss << "title=\"" << docName << "\"";
    oss << " media=" << getMedia();
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


int GSFilter::runGhostscript() {
    Log(L_DEBUG) << "Running GhostScript";
    string outFileOption = "-sOutputFile=" + outFileName;
    string inFileOption = "-f" + inFileName;
    vector<const char *> cmdLine;
    cmdLine.push_back("PS2PDF");
    cmdLine.push_back("-dNOPAUSE");
    cmdLine.push_back("-dBATCH");
    cmdLine.push_back("-dSAFER");
    cmdLine.push_back("-sDEVICE=pdfwrite");
    // TODO: Add include paths for fonts and lib: "-Ipath\\urwfonts;path\\lib"
    if (!extraOptions.empty()) cmdLine.push_back(extraOptions.c_str());
    cmdLine.push_back(outFileOption.c_str());
    cmdLine.push_back("-c");
    cmdLine.push_back(".setpdfwrite");
    cmdLine.push_back(inFileOption.c_str());

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
    int result = gsapi_init_with_args(gsInstance, cmdLine.size(), (char **)cmdLine.data());
    gsapi_exit(gsInstance);
    gsapi_delete_instance(gsInstance);
    Log(L_DEBUG) << "GhostScript returned " << result;
    return result;
}
