/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <fstream>
#include <unistd.h>
#ifdef WIN32
#include <windows.h>
#include <winspool.h>
#else
#include <cups/cups.h>
#include <boost/locale/utf.hpp>
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
    Log(L_DEBUG) << "Installing printer " << printer
                 << (installPrinter(printer, fileName) ? " succeeded" : " failed");
    unlink(fileName.c_str());
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
static bool uninstallPrinter(const wstring & printer, bool cannotFail) {
    HANDLE hPrinter;
    PRINTER_DEFAULTSW pd{ NULL, NULL, PRINTER_ALL_ACCESS };
    if (OpenPrinter((wchar_t *)printer.c_str(), &hPrinter, &pd)) {
        SetPrinter(hPrinter, 0, NULL, PRINTER_CONTROL_PURGE);
        DeletePrinter(hPrinter);
        ClosePrinter(hPrinter);
        Log(L_DEBUG) << "Uninstalled printer " << printer;
    }
    if (!DeletePrinterDriverEx(NULL, NULL, (wchar_t *)printer.c_str(),
                                DPD_DELETE_UNUSED_FILES, 0) && cannotFail) {
        LogError() << "DeletePrinterDriverEx failed";
        return false;
    }
    return true;
}


void PrintManager::handle(const Connection::Ptr & src, const ResetMsgPtr & msg) {
    DWORD needed = 0, returned = 0;
    DWORD flags = PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL;
    EnumPrinters(flags, NULL, 2, NULL, 0, &needed, &returned);
    std::unique_ptr<BYTE[]> buffer(new BYTE[needed]);
    PRINTER_INFO_2W * pri2 = (PRINTER_INFO_2W *)buffer.get();
    if (needed && EnumPrinters(flags, NULL, 2, buffer.get(), needed, &needed, &returned)) {
        for (unsigned int i = 0; i < returned; i++) {
            if (pri2[i].pPortName == wstring(L"flexVDIprint") &&
                pri2[i].pDriverName != wstring(L"flexVDI Printer")) {
                ::uninstallPrinter(pri2[i].pPrinterName, true);
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
    EnumPrinterDrivers(NULL, NULL, 2, NULL, 0, &needed, &returned);
    std::unique_ptr<BYTE[]> buffer(new BYTE[needed]);
    DRIVER_INFO_2W * dinfo = (DRIVER_INFO_2W *)buffer.get();
    if (needed && EnumPrinterDrivers(NULL, NULL, 2, buffer.get(),
                                      needed, &needed, &returned)) {
        for (unsigned int i = 0; i < returned; i++) {
            if (dinfo[i].pName == wstring(L"flexVDI Printer")) {
                GetPrinterDriverDirectory(NULL, NULL, 1, NULL, 0, &needed);
                std::unique_ptr<BYTE[]> bufferDir(new BYTE[needed]);
                if (!needed || !GetPrinterDriverDirectory(NULL, NULL, 1, bufferDir.get(),
                                                           needed, &needed)) {
                    LogError() << "Failed to get Printer Driver Directory";
                    return false;
                }
                wstring driverPath((wchar_t *)bufferDir.get());
                wstring ppdNew = fileAt(ppd, driverPath),
                        config = fileAt(dinfo[i].pConfigFile, driverPath),
                        driver = fileAt(dinfo[i].pDriverPath, driverPath);
                bool result = false;
                if (CopyFile(ppd.c_str(), ppdNew.c_str(), TRUE) &&
                    CopyFile(dinfo[i].pConfigFile, config.c_str(), FALSE) &&
                    CopyFile(dinfo[i].pDriverPath, driver.c_str(), FALSE)) {
                    dinfo[i].pName = (wchar_t *)printer.c_str();
                    dinfo[i].pDataFile = (wchar_t *)ppdNew.c_str();
                    if (AddPrinterDriver(NULL, 2, (LPBYTE)&dinfo[i])) result = true;
                    else LogError() << "AddPrinterDriver failed";
                }
                DeleteFile(ppdNew.c_str());
                DeleteFile(config.c_str());
                DeleteFile(driver.c_str());
                return result;
            }
        }
        LogError() << "No flexVDI printer driver found";
    } else LogError() << "EnumPrinterDrivers failed";
    LogError() << "Failed to install printer driver " << printer;
    return false;
}


static bool installPrinter(const wstring & printer, const wstring & ppd) {
    Log(L_DEBUG) << "Installing printer " << printer << " from " << ppd;

    // Just in case...
    uninstallPrinter(printer, false);

    if (!installPrinterDriver(printer, ppd)) return false;
    DWORD needed = 0, returned = 0;
    DWORD flags = PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL;
    EnumPrinters(flags, NULL, 2, NULL, 0, &needed, &returned);
    std::unique_ptr<BYTE[]> buffer(new BYTE[needed]);
    PRINTER_INFO_2W * pri2 = (PRINTER_INFO_2W *)buffer.get();
    if (needed && EnumPrinters(flags, NULL, 2, buffer.get(), needed, &needed, &returned)) {
        for (unsigned int i = 0; i < returned; i++) {
            if (pri2[i].pDriverName == wstring(L"flexVDI Printer")) {
                pri2[i].pDriverName = pri2[i].pPrinterName = (wchar_t *)printer.c_str();
                if (AddPrinter(NULL, 2, (LPBYTE)&pri2[i])) {
                    Log(L_DEBUG) << "Printer " << printer << " added successfully";
                    return true;
                } else break;
            }
        }
        LogError() << "No flexVDI printer found";
    } else LogError() << "EnumPrinters failed";
    LogError() << "Failed to install printer " << printer;
    uninstallPrinter(printer, false);
    return false;
}


bool PrintManager::installPrinter(const string & printer, const string & ppd) {
    return ::installPrinter(toWstring(printer), toWstring(ppd));
}


bool PrintManager::uninstallPrinter(const string & printer) {
    return ::uninstallPrinter(toWstring(printer), true);
}

#else

void PrintManager::handle(const Connection::Ptr & src, const ResetMsgPtr & msg) {
    cups_dest_t * dests;
    int numDests = cupsGetDests(&dests);
    for (int i = 0; i < numDests; ++i) {
        if (cupsGetOption("flexvdi-shared-printer", dests[i].num_options, dests[i].options)) {
            uninstallPrinter(dests[i].name);
        }
    }
    cupsFreeDests(numDests, dests);
}


static http_t * cupsConnection() {
    return httpConnect2(cupsServer(), ippPort(), NULL, AF_UNSPEC,
                        cupsEncryption(), 1, 30000, NULL);
}


static string getPrinterURI(const string & printer) {
    char uri[HTTP_MAX_URI];
    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, HTTP_MAX_URI, "ipp", NULL, "localhost",
                     ippPort(), "/printers/%s", printer.c_str());
    Log(L_DEBUG) << "URI: " << uri;
    return string(uri);
}


static ipp_t * newRequest(ipp_op_t op, const string & printer) {
    ipp_t * request = ippNewRequest(op);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
                 "printer-uri", NULL, getPrinterURI(printer).c_str());
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, cupsUser());
    return request;
}


bool PrintManager::installPrinter(const string & printer, const string & ppd) {
    Log(L_DEBUG) << "Installing printer " << printer << " from " << ppd;

    int numOptions = 0;
    cups_option_t * options = NULL;
    numOptions = cupsAddOption("device-uri", "flexvdiprint:", numOptions, &options);
    numOptions = cupsAddOption("printer-info", "flexVDI Shared Printer", numOptions, &options);
    numOptions = cupsAddOption("printer-location", "flexVDI Client", numOptions, &options);

    ipp_t * request = newRequest(IPP_OP_CUPS_ADD_MODIFY_PRINTER, printer);
    ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state", IPP_PSTATE_IDLE);
    ippAddBoolean(request, IPP_TAG_PRINTER, "printer-is-accepting-jobs", 1);
    cupsEncodeOptions2(request, numOptions, options, IPP_TAG_OPERATION);
    cupsEncodeOptions2(request, numOptions, options, IPP_TAG_PRINTER);
    cupsFreeOptions(numOptions, options);

    http_t * http = cupsConnection();
    ippDelete(cupsDoFileRequest(http, request, "/admin/", ppd.c_str()));
    if (cupsLastError() > IPP_STATUS_OK_CONFLICTING) {
        Log(L_ERROR) << "Failed to install printer: " << cupsLastErrorString();
        httpClose(http);
        return false;
    }

    cups_dest_t * dest, * dests;
    int numDests = cupsGetDests(&dests);
    dest = cupsGetDest(printer.c_str(), NULL, numDests, dests);
    if (!dest) {
        Log(L_ERROR) << "Failed to install printer";
        return false;
    }
    dest->num_options = cupsAddOption("flexvdi-shared-printer", "true",
                                      dest->num_options, &dest->options);
    cupsSetDests(numDests, dests);
    cupsFreeDests(numDests, dests);
    httpClose(http);
    return true;
}


bool PrintManager::uninstallPrinter(const string & printer) {
    bool result = true;
    ipp_t * request = newRequest(IPP_OP_CUPS_DELETE_PRINTER, printer);
    http_t * http = cupsConnection();
    ippDelete(cupsDoRequest(http, request, "/admin/"));
    if (cupsLastError() > IPP_STATUS_OK_CONFLICTING) {
        Log(L_ERROR) << "Failed to uninstall printer: " << cupsLastErrorString();
        result = false;
    }
    httpClose(http);
    return result;
}

#endif
