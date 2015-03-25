/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <windows.h>
#include <winspool.h>
#include <wctype.h>
#include "PrintManager.hpp"
#include "util.hpp"

using namespace flexvm;
using std::string;
using std::wstring;


class wsbuffer {
public:
    wsbuffer(const wchar_t * copy) {
        size_t len = std::wcslen(copy);
        buffer.reset(new wchar_t[len + 1]);
        std::wcscpy(buffer.get(), copy);
    }
    wsbuffer(const wstring & copy) : wsbuffer(copy.c_str()) {}
    operator wchar_t *() const { return buffer.get(); }
    operator wstring() const { return wstring(buffer.get()); }
    wsbuffer & operator+=(const wsbuffer & r) {
        size_t lenThis = std::wcslen(*this), len = lenThis + std::wcslen(r);
        wchar_t * tmp = new wchar_t[(len + 1)*sizeof(wchar_t)];
        std::wcscpy(tmp, *this);
        std::wcscpy(tmp + lenThis, r);
        buffer.reset(tmp);
        return *this;
    }
    friend Log & operator<<(Log & os, const wsbuffer & s) {
        return os << s.buffer.get();
    }
    bool isEqualTo(const wchar_t * r) const {
        return std::wcscmp(*this, r) == 0;
    }

private:
    std::unique_ptr<wchar_t[]> buffer;
};


static const wsbuffer portName(L"flexVDIprint");
static const wsbuffer monitorName(L"flexVDI Redirection Port");
static const wsbuffer pdfPrinter(L"flexVDI PDF Printer");
static const wsbuffer tag(L"flexVDI shared printer");


static std::shared_ptr<PRINTER_INFO_2> getPrinters(DWORD & numPrinters) {
    DWORD needed = 0;
    DWORD flags = PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL;
    EnumPrinters(flags, NULL, 2, NULL, 0, &needed, &numPrinters);
    std::shared_ptr<BYTE> buffer(new BYTE[needed], std::default_delete<BYTE[]>());
    if (needed && EnumPrinters(flags, NULL, 2, buffer.get(), needed, &needed, &numPrinters)) {
        return std::shared_ptr<PRINTER_INFO_2>(buffer, (PRINTER_INFO_2 *)buffer.get());
    } else {
        LogError() << "EnumPrinters failed";
        numPrinters = 0;
        return std::shared_ptr<PRINTER_INFO_2>();
    }
}


static bool uninstallPrinterDriver(wchar_t * printer, bool cannotFail) {
    if (!DeletePrinterDriverEx(NULL, NULL, printer, DPD_DELETE_UNUSED_FILES, 0) &&
            cannotFail) {
        LogError() << "DeletePrinterDriverEx failed";
        return false;
    }
    return true;
}


static bool uninstallPrinter(PRINTER_INFO_2 * pinfo) {
    HANDLE hPrinter;
    PRINTER_DEFAULTS pd{ NULL, NULL, PRINTER_ALL_ACCESS };
    if (OpenPrinter(pinfo->pPrinterName, &hPrinter, &pd)) {
        SetPrinter(hPrinter, 0, NULL, PRINTER_CONTROL_PURGE);
        DeletePrinter(hPrinter);
        ClosePrinter(hPrinter);
        Log(L_DEBUG) << "Uninstalled printer " << pinfo->pPrinterName;
    }
    return uninstallPrinterDriver(pinfo->pDriverName, true);
}


static void restartSpooler() {
    SC_HANDLE serviceCM, spooler;
    if ((serviceCM = OpenSCManager(0, 0, SC_MANAGER_CONNECT))) {
        if ((spooler = OpenService(serviceCM, L"Spooler", SERVICE_ALL_ACCESS))) {
            SERVICE_STATUS status;
            ControlService(spooler, SERVICE_CONTROL_STOP, &status);
            for (int i = 0; status.dwCurrentState != SERVICE_STOPPED && i < 10; ++i) {
                Sleep(100);
                QueryServiceStatus(spooler, &status);
            }
            StartService(spooler, 0, NULL);
            for (int i = 0; status.dwCurrentState != SERVICE_RUNNING && i < 10; ++i) {
                Sleep(100);
                QueryServiceStatus(spooler, &status);
            }
            CloseServiceHandle(spooler);
        }
        CloseServiceHandle(serviceCM);
    }
}


void PrintManager::handle(const Connection::Ptr & src, const ResetMsgPtr & msg) {
    restartSpooler();
    DWORD numPrinters;
    std::shared_ptr<PRINTER_INFO_2> printers = getPrinters(numPrinters);
    PRINTER_INFO_2 * pinfo = printers.get();
    for (unsigned int i = 0; i < numPrinters; i++) {
        if (portName.isEqualTo(pinfo[i].pPortName) &&
            !pdfPrinter.isEqualTo(pinfo[i].pDriverName)) {
            ::uninstallPrinter(&pinfo[i]);
        }
    }
}


static wstring fileAt(const wstring & srcFile, const wstring & dstDir) {
    int withSlash = dstDir.back() == '\\' ? 1 : 0;
    return dstDir + srcFile.substr(srcFile.find_last_of('\\') + withSlash);
}


static bool installPrinterDriver(const wsbuffer & printer, const wsbuffer & ppd) {
    DWORD needed = 0, returned = 0;
    EnumPrinterDrivers(NULL, NULL, 2, NULL, 0, &needed, &returned);
    std::unique_ptr<BYTE[]> buffer(new BYTE[needed]);
    DRIVER_INFO_2W * dinfo = (DRIVER_INFO_2W *)buffer.get();
    if (needed && EnumPrinterDrivers(NULL, NULL, 2, buffer.get(),
                                     needed, &needed, &returned)) {
        for (unsigned int i = 0; i < returned; i++) {
            wstring driverName = dinfo[i].pDriverPath;
            if (driverName.length() < 12) continue;
            driverName = driverName.substr(driverName.length() - 12);
            for (wchar_t & c : driverName) {
                c = towupper(c);
            }
            if (driverName == L"PSCRIPT5.DLL") {
                GetPrinterDriverDirectory(NULL, NULL, 1, NULL, 0, &needed);
                std::unique_ptr<BYTE[]> bufferDir(new BYTE[needed]);
                if (!needed || !GetPrinterDriverDirectory(NULL, NULL, 1, bufferDir.get(),
                                                          needed, &needed)) {
                    LogError() << "Failed to get Printer Driver Directory";
                    return false;
                }
                wstring driverPath((wchar_t *)bufferDir.get());
                wsbuffer ppdNew = fileAt(ppd, driverPath),
                         config = fileAt(dinfo[i].pConfigFile, driverPath),
                         driver = fileAt(dinfo[i].pDriverPath, driverPath);
                bool result = false;
                if (CopyFile(ppd, ppdNew, TRUE) &&
                    CopyFile(dinfo[i].pConfigFile, config, FALSE) &&
                    CopyFile(dinfo[i].pDriverPath, driver, FALSE)) {
                    dinfo[i].pName = printer;
                    dinfo[i].pDataFile = ppdNew;
                    if (AddPrinterDriver(NULL, 2, (LPBYTE)&dinfo[i])) result = true;
                    else LogError() << "AddPrinterDriver failed";
                }
                DeleteFile(ppdNew);
                DeleteFile(config);
                DeleteFile(driver);
                return result;
            }
        }
        LogError() << "No pscript5 printer driver found";
    } else LogError() << "EnumPrinterDrivers failed";
    LogError() << "Failed to install printer driver " << printer;
    return false;
}


static bool installPrinter(const wsbuffer & printer, const wsbuffer & ppd) {
    Log(L_DEBUG) << "Installing printer " << printer << " from " << ppd;

    if (!installPrinterDriver(printer, ppd)) return false;

    wsbuffer comment(toWstring(PrintManager::sharedPrinterDescription).c_str());
    wsbuffer location(toWstring(PrintManager::sharedPrinterLocation).c_str());
    wsbuffer printProcessor(L"WinPrint");
    wsbuffer dataType(L"RAW");
    PRINTER_INFO_2 pinfo;
    std::fill_n((uint8_t *)&pinfo, sizeof(PRINTER_INFO_2), 0);
    pinfo.pDriverName = pinfo.pPrinterName = printer;
    pinfo.pPortName = portName;
    pinfo.pComment = comment;
    pinfo.pLocation = location;
    pinfo.pPrintProcessor = printProcessor;
    pinfo.pDatatype = dataType;

    HANDLE hprinter = AddPrinter(NULL, 2, (LPBYTE)&pinfo);
    if (hprinter) {
        Log(L_DEBUG) << "Printer " << printer << " added successfully";
        LPBYTE buffer = (LPBYTE)(wchar_t *)printer;
        size_t size = (std::wcslen(printer) + 1) * sizeof(wchar_t);
        SetPrinterData(hprinter, tag, REG_SZ, buffer, size);
        ClosePrinter(hprinter);
        return true;
    } else {
        LogError() << "Failed to install new printer";
        uninstallPrinterDriver(printer, false);
        return false;
    }
}


bool PrintManager::installPrinter(const string & printer, const string & ppd) {
    // Just in case...
    uninstallPrinter(printer);
    return ::installPrinter(toWstring(printer), toWstring(ppd));
}


bool PrintManager::uninstallPrinter(const string & printer) {
    wstring wPrinter = toWstring(printer);
    DWORD numPrinters;
    std::shared_ptr<PRINTER_INFO_2> printers = getPrinters(numPrinters);
    PRINTER_INFO_2 * pinfo = printers.get();
    for (unsigned int i = 0; i < numPrinters; ++i, ++pinfo) {
        HANDLE hprinter;
        if (OpenPrinter(pinfo->pPrinterName, &hprinter, NULL)) {
            DWORD needed = 0, type;
            GetPrinterData(hprinter, tag, &type, NULL, 0, &needed);
            std::unique_ptr<BYTE[]> buffer(new BYTE[needed]);
            wchar_t * name = (wchar_t *)buffer.get();
            DWORD result = GetPrinterData(hprinter, tag, &type,
                                          buffer.get(), needed, &needed);
            ClosePrinter(hprinter);
            if (needed && result == ERROR_SUCCESS && type == REG_SZ && wPrinter == name)
                return ::uninstallPrinter(pinfo);
        }
    }
    Log(L_INFO) << "Printer " << printer << " not found";
    return true;
}


static bool isMonitorInstalled() {
    DWORD needed = 0, returned = 0;
    EnumMonitors(NULL, 1, NULL, 0, &needed, &returned);
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[needed]);
    MONITOR_INFO_1W * mi = (MONITOR_INFO_1W *)buffer.get();
    return_if(needed <= 0 || !EnumMonitors(NULL, 1, (LPBYTE)mi, needed, &needed, &returned),
              "EnumMonitorsW failed", false);
    for (DWORD i = 0; i < returned; i++) {
        if (monitorName.isEqualTo(mi[i].pName)) {
            return true;
        }
    }
    return false;
}


static bool installMonitor(const wsbuffer & dll) {
    if (!isMonitorInstalled()) {
        MONITOR_INFO_2W mi2;
        mi2.pName = monitorName;
        mi2.pDLLName = dll;
        mi2.pEnvironment = NULL;
        return_if(!AddMonitor(NULL, 2, (LPBYTE)&mi2), "AddMonitorW failed", false);
    }
    return true;
}


static bool addPort(const wchar_t * portName) {
    HANDLE hMonitor = INVALID_HANDLE_VALUE;
    PRINTER_DEFAULTS pdef = { NULL, NULL, SERVER_ACCESS_ADMINISTER };
    wsbuffer monitorString(L",XcvMonitor ");
    monitorString += monitorName;
    DWORD xcvSize = (std::wcslen(portName) + 1) * sizeof(wchar_t);

    return_if(!OpenPrinter(monitorString, &hMonitor, &pdef), "OpenPrinterW failed", false);

    DWORD status, needed;
    bool success = XcvData(hMonitor, L"AddPort", (BYTE *)portName, xcvSize,
                           NULL, 0, &needed, &status);
    success = success && (status == NO_ERROR || status == ERROR_ALREADY_EXISTS);
    if (!success)
        LogError() << "XcvDataWProc failed";

    ClosePrinter(hMonitor);
    return success;
}


bool installFollowMePrinting(const char * portDll, const char * ppd) {
    return installMonitor(toWstring(portDll).c_str()) &&
            addPort(portName) &&
            installPrinter(pdfPrinter, toWstring(ppd));
}


static bool deletePort(const wsbuffer & portName) {
    DWORD numPrinters;
    std::shared_ptr<PRINTER_INFO_2> printers = getPrinters(numPrinters);
    PRINTER_INFO_2 * pinfo = printers.get();
    for (unsigned int i = 0; i < numPrinters; ++i, ++pinfo) {
        if (portName.isEqualTo(pinfo->pPortName)) {
            uninstallPrinter(pinfo);
        }
    }
    /* This may not be silent, but probably is */
    DeletePort(NULL, HWND_DESKTOP, portName);
    return true;
}


bool uninstallFollowMePrinting() {
    if (!isMonitorInstalled()) return true;

    DWORD needed = 0, returned = 0;
    /* Check if monitor is still in use */
    EnumPorts(NULL, 2, NULL, 0, &needed, &returned);
    std::unique_ptr<BYTE[]> buffer(new BYTE[needed]);
    PORT_INFO_2W * pi = (PORT_INFO_2W *)buffer.get();
    return_if(!needed || !EnumPorts(NULL, 2, buffer.get(), needed, &needed, &returned),
              "EnumPortsW failed", false);
    for (DWORD i = 0; i < returned; i++) {
        if (monitorName.isEqualTo(pi[i].pMonitorName)) {
            deletePort(pi[i].pPortName);
        }
    }

    /* Try again, hoping that we succeeded in deleting all ports/printers */
    return_if(!EnumPorts(NULL, 2, buffer.get(), needed, &needed, &returned),
              "EnumPortsW failed", false);
    for (unsigned int i = 0; i < returned; i++) {
        return_if(monitorName.isEqualTo(pi[i].pMonitorName), "Monitor still in use", false);
    }

    /* Try to delete the monitor */
    return_if(DeleteMonitor(NULL, NULL, monitorName), "DeleteMonitor failed", false);

    return true;
}
