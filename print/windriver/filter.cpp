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
    shared_ptr<PRINTER_INFO_2A> printerInfo;

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
        tmpFiles.push_back(flexvm::getTempFileName("fpj")); // fpj = FlexVDI Print Job
        return tmpFiles.back();
    }

    void readEnv() {
        std::unique_ptr<wchar_t[]> wbuffer;
        int size;

        size = GetEnvironmentVariableW(L"REDMON_JOB", NULL, 0);
        wbuffer.reset(new wchar_t[size]);
        GetEnvironmentVariableW(L"REDMON_JOB", wbuffer.get(), size);
        jobId = stoi(wbuffer.get());

        size = GetEnvironmentVariableW(L"REDMON_PRINTER", NULL, 0);
        wbuffer.reset(new wchar_t[size]);
        GetEnvironmentVariableW(L"REDMON_PRINTER", wbuffer.get(), size);
        printerName = toString(wbuffer.get());

        size = GetEnvironmentVariableW(L"REDMON_DOCNAME", NULL, 0);
        wbuffer.reset(new wchar_t[size]);
        GetEnvironmentVariableW(L"REDMON_DOCNAME", wbuffer.get(), size);
        docName = toString(wbuffer.get());
    }

    void getJobInfo();
    void getPrinterInfo();
    int getPrinterCap(WORD cap, char * buffer);
    string getMedia();
    int getMediaType();
    int getMediaSource();
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
    getPrinterInfo();
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
        if (needed > 0) {
            std::shared_ptr<char> buffer(new char[needed], std::default_delete<char[]>());
            if(GetJobA(printer, jobId, 2, (LPBYTE)buffer.get(), needed, &needed)) {
                jobInfo = shared_ptr<JOB_INFO_2A>(buffer, (JOB_INFO_2A *)buffer.get());
            }
        }
        ClosePrinter(printer);
    }
}


void GSFilter::getPrinterInfo() {
    HANDLE printer;
    if (OpenPrinterA((char *)printerName.c_str(), &printer, NULL)) {
        DWORD needed;
        GetPrinterA(printer, 2, NULL, 0, &needed);
        if (needed > 0) {
            std::shared_ptr<char> buffer(new char[needed], std::default_delete<char[]>());
            if (GetPrinterA(printer, 2, (LPBYTE)buffer.get(), needed, &needed)) {
                printerInfo = shared_ptr<PRINTER_INFO_2A>(buffer, (PRINTER_INFO_2A *)buffer.get());
            }
        }
        ClosePrinter(printer);
    }
}


int GSFilter::getPrinterCap(WORD cap, char * buffer) {
    return DeviceCapabilitiesA(printerInfo->pPrinterName, printerInfo->pPortName,
                               cap, buffer, printerInfo->pDevMode);
}


string GSFilter::getMedia() {
    DEVMODEA * devMode = jobInfo->pDevMode;
    switch (devMode->dmPaperSize) {
        case DMPAPER_TABLOID: return "Tabloid";
        case DMPAPER_LEDGER: return "Ledger";
        case DMPAPER_LEGAL: return "Legal";
        case DMPAPER_LETTER: return "Letter";
        case DMPAPER_LETTERSMALL: return "LetterSmall";
        case DMPAPER_STATEMENT: return "Statement";
        case DMPAPER_NOTE: return "Note";
//         case DMPAPER_A0: return "A0";
//         case DMPAPER_A1: return "A1";
        case DMPAPER_A2: return "A2";
        case DMPAPER_A3: return "A3";
        case DMPAPER_A4: return "A4";
        case DMPAPER_A4SMALL: return "A4Small";
        case DMPAPER_A5: return "A5";
        case DMPAPER_A6: return "A6";
//         case DMPAPER_A7: return "A7";
//         case DMPAPER_A8: return "A8";
//         case DMPAPER_A9: return "A9";
//         case DMPAPER_A10: return "A10";
        case DMPAPER_B4: return "B4";
        case DMPAPER_B5: return "B5";
//         case DMPAPER_B6: return "B6";
        case DMPAPER_ENV_C3: return "EnvC3";
        case DMPAPER_ENV_C4: return "EnvC4";
        case DMPAPER_ENV_C5: return "EnvC5";
        case DMPAPER_ENV_C6: return "EnvC6";
        default: {
            ostringstream media;
            media << devMode->dmPaperWidth << 'x' << devMode->dmPaperLength;
            return media.str();
        }
    }
}


int GSFilter::getMediaSource() {
    DEVMODEA * devMode = jobInfo->pDevMode;
    int source = devMode->dmDefaultSource - DMBIN_USER;
    int numSources = getPrinterCap(DC_BINNAMES, NULL);
    // FIXME: Windows introduces "Automatic selection" as first source
    return source > 0 && source < numSources ? source - 1 : 0;
}


int GSFilter::getMediaType() {
    DEVMODEA * devMode = jobInfo->pDevMode;
    int mediaType = devMode->dmMediaType - DMMEDIA_USER;
    int numMediaTypes = getPrinterCap(DC_MEDIATYPENAMES, NULL);
    return mediaType >= 0 && mediaType < numMediaTypes ? mediaType : 0;
}


string GSFilter::getJobOptions() {
    DEVMODEA * devMode = jobInfo->pDevMode;
    ostringstream oss;
    oss << "printer=\"" << printerName << "\"";
    oss << " title=\"" << docName << "\"";
    oss << " media=" << getMedia();
    oss << " media-source=" << getMediaSource();
    oss << " media-type=" << getMediaType();
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
    oss << (devMode->dmColor == DMCOLOR_COLOR ? " color" : " gray");
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
    cmdLine.push_back("-dPDFSETTINGS=/printer");
    cmdLine.push_back("-dCompressFonts=true");
    cmdLine.push_back("-dSubsetFonts=true");
    cmdLine.push_back("-dEmbedAllFonts=true");
    // TODO: Add include paths for fonts and lib: "-Ipath\\urwfonts;path\\lib"
    if (!extraOptions.empty()) cmdLine.push_back(extraOptions.c_str());
    cmdLine.push_back(outFileOption.c_str());
    cmdLine.push_back("-c");
    cmdLine.push_back(".setpdfwrite <</NeverEmbed [ ]>> setdistillerparams");
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
