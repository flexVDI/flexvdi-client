/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <fstream>
#ifdef WIN32
#include <windows.h>
#include <winspool.h>
#else
#endif
#include "PrintManager.hpp"
#include "FlexVDIGuestAgent.hpp"
#include "util.hpp"

using namespace flexvm;
using std::string;
using std::wstring;
namespace ph = std::placeholders;


REGISTER_COMPONENT(PrintManager);


void PrintManager::handle(const Connection::Ptr & src, const PrintJobMsgPtr & msg) {
    Log(L_DEBUG) << "Ready to send a new print job";
    jobs.emplace_back(src, getNewId());
    Job & job = jobs.back();
    msg->id = job.id;
    src->registerErrorHandler(std::bind(&PrintManager::closed, this, ph::_1, ph::_2));
    Connection::Ptr client = FlexVDIGuestAgent::singleton().spiceClient();
    client->send(MessageBuffer(msg));
}


void PrintManager::handle(const Connection::Ptr & src, const PrintJobDataMsgPtr & msg) {
    auto job = std::find_if(jobs.begin(), jobs.end(),
                            [&src](const Job & j) { return src == j.src; });
    if (job == jobs.end()) {
        Log(L_WARNING) << "Received print job data for an unknown job";
        return;
    }
    msg->id = job->id;
    Connection::Ptr client = FlexVDIGuestAgent::singleton().spiceClient();
    client->send(MessageBuffer(msg));
}


void PrintManager::handle(const Connection::Ptr & src, const SharePrinterMsgPtr & msg) {
    string printer(getPrinterName(msg.get()));
    const char * ppdText = getPPD(msg.get()), * ppdEnd = ppdText + msg->ppdLength;
    const char * key = "*PCFileName: \"";
    const int keyLength = 14;
    string fileName;
    for (const char * pos = ppdText; pos < ppdEnd - keyLength; ++pos) {
        if (!strncmp(pos, key, keyLength)) {
            pos += keyLength;
            const char * end = strchr(pos, '"');
            if (end) {
                fileName = getTempDirName() + string(pos, end);
                break;
            }
        }
    }
    if (fileName.empty()) fileName = getTempFileName("fv") + ".ppd";
    std::ofstream ppdFile(fileName.c_str());
    ppdFile.write(ppdText, msg->ppdLength);
    installPrinter(printer, fileName);
}


void PrintManager::handle(const Connection::Ptr & src, const UnsharePrinterMsgPtr & msg) {
    string printer(msg->printerName, msg->printerNameLength);
    Log(L_DEBUG) << "Uninstalling printer " << printer
                 << (uninstallPrinter(printer) ? " succeeded" : " failed");
}


void PrintManager::closed(const Connection::Ptr & src,
                          const boost::system::error_code & error) {
    auto job = std::find_if(jobs.begin(), jobs.end(),
                            [&src](const Job & j) { return src == j.src; });
    if (job == jobs.end()) {
        Log(L_WARNING) << "No print job associated with this connection";
        return;
    }
    Log(L_DEBUG) << "Finished sending the PDF file, remove the job";
    // Send a last block of 0 bytes to signal the end of the job
    MessageBuffer buffer(FLEXVDI_PRINTJOBDATA, sizeof(FlexVDIPrintJobDataMsg));
    auto msg = (FlexVDIPrintJobDataMsg *)buffer.getMsgData();
    msg->id = job->id;
    msg->dataLength = 0;
    Connection::Ptr client = FlexVDIGuestAgent::singleton().spiceClient();
    client->send(buffer);
    jobs.erase(job);
}


#ifdef WIN32
static bool uninstallPrinterW(const wstring & printer, bool cannotFail) {
    HANDLE hPrinter;
    PRINTER_DEFAULTSW pd{ NULL, NULL, PRINTER_ALL_ACCESS };
    if (OpenPrinterW((wchar_t *)printer.c_str(), &hPrinter, &pd)) {
        DeletePrinter(hPrinter);
        ClosePrinter(hPrinter);
        Log(L_DEBUG) << "Uninstalled printer " << printer;
    }
    if (!DeletePrinterDriverExW(NULL, NULL, (wchar_t *)printer.c_str(),
                                DPD_DELETE_UNUSED_FILES, 0) && cannotFail) {
        LogError() << "DeletePrinterDriverEx failed";
        return false;
    }
    return true;
}


void PrintManager::handle(const Connection::Ptr & src, const ResetMsgPtr & msg) {
    DWORD needed = 0, returned = 0;
    DWORD flags = PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL;
    EnumPrintersW(flags, NULL, 2, NULL, 0, &needed, &returned);
    std::unique_ptr<BYTE[]> buffer(new BYTE[needed]);
    PRINTER_INFO_2W * pri2 = (PRINTER_INFO_2W *)buffer.get();
    if (needed && EnumPrintersW(flags, NULL, 2, buffer.get(), needed, &needed, &returned)) {
        for (unsigned int i = 0; i < returned; i++) {
            if (pri2[i].pPortName == wstring(L"flexVDIprint") &&
                pri2[i].pDriverName != wstring(L"flexVDI Printer")) {
                uninstallPrinterW(pri2[i].pPrinterName, true);
            }
        }
    } else LogError() << "EnumPrinters failed";
}


static wstring fileAt(const wstring & srcFile, const wstring & dstDir) {
    int withSlash = dstDir.back() == '\\' ? 1 : 0;
    return dstDir + srcFile.substr(srcFile.find_last_of('\\') + withSlash);
}


static bool installPrinterDriver(const wstring & printer, const wstring & ppd) {
    DWORD needed = 0, returned = 0;
    EnumPrinterDriversW(NULL, NULL, 2, NULL, 0, &needed, &returned);
    std::unique_ptr<BYTE[]> buffer(new BYTE[needed]);
    DRIVER_INFO_2W * dinfo = (DRIVER_INFO_2W *)buffer.get();
    if (needed && EnumPrinterDriversW(NULL, NULL, 2, buffer.get(),
                                      needed, &needed, &returned)) {
        for (unsigned int i = 0; i < returned; i++) {
            if (dinfo[i].pName == wstring(L"flexVDI Printer")) {
                GetPrinterDriverDirectoryW(NULL, NULL, 1, NULL, 0, &needed);
                std::unique_ptr<BYTE[]> bufferDir(new BYTE[needed]);
                if (!needed || !GetPrinterDriverDirectoryW(NULL, NULL, 1, bufferDir.get(),
                                                           needed, &needed)) {
                    LogError() << "Failed to get Printer Driver Directory";
                    return false;
                }
                wstring driverPath((wchar_t *)bufferDir.get());
                wstring ppdNew = fileAt(ppd, driverPath),
                        config = fileAt(dinfo[i].pConfigFile, driverPath),
                        driver = fileAt(dinfo[i].pDriverPath, driverPath);
                bool result = false;
                if (CopyFileW(ppd.c_str(), ppdNew.c_str(), TRUE) &&
                    CopyFileW(dinfo[i].pConfigFile, config.c_str(), FALSE) &&
                    CopyFileW(dinfo[i].pDriverPath, driver.c_str(), FALSE)) {
                    dinfo[i].pName = (wchar_t *)printer.c_str();
                    dinfo[i].pDataFile = (wchar_t *)ppdNew.c_str();
                    if (AddPrinterDriverW(NULL, 2, (LPBYTE)&dinfo[i])) result = true;
                    else LogError() << "AddPrinterDriver failed";
                }
                DeleteFileW(ppdNew.c_str());
                DeleteFileW(config.c_str());
                DeleteFileW(driver.c_str());
                return result;
            }
        }
        LogError() << "No flexVDI printer driver found";
    } else LogError() << "EnumPrinterDrivers failed";
    LogError() << "Failed to install printer driver " << printer;
    return false;
}


static bool installPrinterW(const wstring & printer, const wstring & ppd) {
    Log(L_DEBUG) << "Installing printer " << printer << " from " << ppd;

    // Just in case...
    uninstallPrinterW(printer, false);

    if (!installPrinterDriver(printer, ppd)) return false;
    DWORD needed = 0, returned = 0;
    DWORD flags = PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL;
    EnumPrintersW(flags, NULL, 2, NULL, 0, &needed, &returned);
    std::unique_ptr<BYTE[]> buffer(new BYTE[needed]);
    PRINTER_INFO_2W * pri2 = (PRINTER_INFO_2W *)buffer.get();
    if (needed && EnumPrintersW(flags, NULL, 2, buffer.get(), needed, &needed, &returned)) {
        for (unsigned int i = 0; i < returned; i++) {
            if (pri2[i].pDriverName == wstring(L"flexVDI Printer")) {
                pri2[i].pDriverName = pri2[i].pPrinterName = (wchar_t *)printer.c_str();
                if (AddPrinterW(NULL, 2, (LPBYTE)&pri2[i])) {
                    Log(L_DEBUG) << "Printer " << printer << " added successfully";
                    return true;
                } else break;
            }
        }
        LogError() << "No flexVDI printer found";
    } else LogError() << "EnumPrinters failed";
    LogError() << "Failed to install printer " << printer;
    uninstallPrinterW(printer, false);
    return false;
}


bool PrintManager::installPrinter(const string & printer, const string & ppd) {
    return installPrinterW(toWstring(printer), toWstring(ppd));
}


bool PrintManager::uninstallPrinter(const string & printer) {
    return uninstallPrinterW(toWstring(printer), true);
}


#else
void PrintManager::handle(const Connection::Ptr & src, const ResetMsgPtr & msg) {
}


bool PrintManager::installPrinter(const string & printer, const string & ppd) {
    return false;
}


bool PrintManager::uninstallPrinter(const string & printer) {
    return false;
}

#endif
