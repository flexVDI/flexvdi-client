/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <windows.h>
#include <winspool.h>
#include "PrintManager.hpp"
#include "util.hpp"

using namespace flexvm;
using std::string;
using std::wstring;


std::shared_ptr<PRINTER_INFO_2> getPrinters(DWORD & numPrinters) {
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
    if (!DeletePrinterDriverEx(NULL, NULL, printer, DPD_DELETE_UNUSED_FILES, 0) && cannotFail) {
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
        if (pinfo[i].pPortName == wstring(L"flexVDIprint") &&
            pinfo[i].pDriverName != wstring(L"flexVDI Printer")) {
            ::uninstallPrinter(&pinfo[i]);
        }
    }
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


static bool clonePrinter(const wstring & printer, PRINTER_INFO_2 * pinfo) {
    pinfo->pDriverName = pinfo->pPrinterName = (wchar_t *)printer.c_str();
    wstring comment(toWstring(PrintManager::sharedPrinterDescription));
    wstring location(toWstring(PrintManager::sharedPrinterLocation));
    pinfo->pComment = (wchar_t *)comment.c_str();
    pinfo->pLocation = (wchar_t *)location.c_str();
    pinfo->Attributes &= ~PRINTER_ATTRIBUTE_SHARED;
    HANDLE hprinter = AddPrinter(NULL, 2, (LPBYTE)pinfo);
    if (hprinter) {
        Log(L_DEBUG) << "Printer " << printer << " added successfully";
        SetPrinterData(hprinter, (wchar_t *)L"flexVDI shared printer", REG_SZ,
                       (LPBYTE)printer.c_str(), (printer.length() + 1) * sizeof(wchar_t));
        ClosePrinter(hprinter);
        return true;
    } else return false;
}


static bool installPrinter(const wstring & printer, const wstring & ppd) {
    Log(L_DEBUG) << "Installing printer " << printer << " from " << ppd;

    if (!installPrinterDriver(printer, ppd)) return false;

    DWORD needed = 0;
    GetDefaultPrinter(NULL, &needed);
    std::unique_ptr<wchar_t[]> defaultPrinter(new wchar_t[needed]);
    bool resetDefault = needed && GetDefaultPrinter(defaultPrinter.get(), &needed);

    DWORD numPrinters;
    std::shared_ptr<PRINTER_INFO_2> printers = getPrinters(numPrinters);
    PRINTER_INFO_2 * pinfo = printers.get();
    bool found = false;
    for (unsigned int i = 0; i < numPrinters && !found; ++i, ++pinfo) {
        found = pinfo->pDriverName == wstring(L"flexVDI Printer");
    }
    if (!found) {
        LogError() << "No flexVDI printer found";
    } else if (clonePrinter(printer, --pinfo)) {
        if (resetDefault) {
            Log(L_DEBUG) << "Reseting default printer to " << defaultPrinter.get();
            SetDefaultPrinter(defaultPrinter.get());
        }
        return true;
    } else {
        LogError() << "Failed to install new printer";
    }
    uninstallPrinterDriver((wchar_t *)printer.c_str(), false);
    return false;
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
            GetPrinterData(hprinter, (wchar_t *)L"flexVDI shared printer",
                           &type, NULL, 0, &needed);
            std::unique_ptr<BYTE[]> buffer(new BYTE[needed]);
            wchar_t * name = (wchar_t *)buffer.get();
            DWORD result = GetPrinterData(hprinter, (wchar_t *)L"flexVDI shared printer",
                                          &type, buffer.get(), needed, &needed);
            ClosePrinter(hprinter);
            if (needed && result == ERROR_SUCCESS && type == REG_SZ && wPrinter == name)
                return ::uninstallPrinter(pinfo);
        }
    }
    Log(L_INFO) << "Printer " << printer << " not found";
    return true;
}
