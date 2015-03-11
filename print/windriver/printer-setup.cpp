/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <windows.h>
#include <winspool.h>
#include <shellapi.h>
#include <string>
#include <memory>
#include <system_error>


class wsbuffer {
public:
    wsbuffer(const wchar_t * copy) {
        size_t len = std::wcslen(copy);
        buffer.reset(new wchar_t[(len + 1)*sizeof(wchar_t)]);
        std::wcscpy(buffer.get(), copy);
    }
    operator wchar_t *() const { return buffer.get(); }
    wsbuffer & operator+=(const wsbuffer & r) {
        size_t lenThis = std::wcslen(*this), len = lenThis + std::wcslen(r);
        wchar_t * tmp = new wchar_t[(len + 1)*sizeof(wchar_t)];
        std::wcscpy(tmp, *this);
        std::wcscpy(tmp + lenThis, r);
        buffer.reset(tmp);
        return *this;
    }
    bool isEqualTo(const wchar_t * r) const {
        return std::wcscmp(*this, r) == 0;
    }

private:
    std::unique_ptr<wchar_t[]> buffer;
};


class PortInstaller {
public:
    PortInstaller(const wchar_t * mon, wchar_t * error) : monitorName(mon), error(error) {}

    bool installMonitor(const wchar_t * dll);
    bool uninstallMonitor();
    bool isInstalled();
    bool addPort(const wchar_t * portName);
    bool deletePort(const wchar_t * portName);

    void reportError(const wchar_t * e) {
        std::wcscpy(error, e);
    }

private:
    wsbuffer monitorName;
    wchar_t * error;
};


bool PortInstaller::installMonitor(const wchar_t * dll) {
    if (!isInstalled()) {
        wsbuffer monitorDll(dll);
        MONITOR_INFO_2W mi2;
        mi2.pName = monitorName;
        mi2.pDLLName = monitorDll;
        mi2.pEnvironment = NULL;
        if (!AddMonitor(NULL, 2, (LPBYTE)&mi2)) {
            reportError(L"AddMonitorW failed");
            return false;
        }
    }
    return true;
}


bool PortInstaller::isInstalled() {
    DWORD needed = 0, returned = 0;
    EnumMonitors(NULL, 1, NULL, 0, &needed, &returned);
    if (needed > 0) {
        std::unique_ptr<uint8_t[]> buffer(new uint8_t[needed]);
        MONITOR_INFO_1W * mi = (MONITOR_INFO_1W *)buffer.get();
        if (EnumMonitors(NULL, 1, (LPBYTE)mi, needed, &needed, &returned)) {
            for (DWORD i = 0; i < returned; i++) {
                if (monitorName.isEqualTo(mi[i].pName)) {
                    return true;
                }
            }
        } else reportError(L"EnumMonitorsW failed");
    } else reportError(L"EnumMonitorsW failed");
    return false;
}


bool PortInstaller::addPort(const wchar_t * portName) {
    HANDLE hPrinter = INVALID_HANDLE_VALUE;
    PRINTER_DEFAULTS pdef = { NULL, NULL, SERVER_ACCESS_ADMINISTER };
    wsbuffer monitorString(L",XcvMonitor ");
    monitorString += monitorName;
    DWORD xcvSize = (std::wcslen(portName) + 1) * sizeof(wchar_t);

    if (!OpenPrinter(monitorString, &hPrinter, &pdef)) {
        reportError(L"OpenPrinterW failed");
        return false;
    }

    bool result = false;
    DWORD status, needed;
    if (!XcvData(hPrinter, L"AddPort", (BYTE *)portName, xcvSize, NULL, 0, &needed, &status)) {
        reportError(L"XcvDataWProc failed");
    } else if (status != NO_ERROR && status != ERROR_ALREADY_EXISTS) {
        reportError(L"XcvDataWProc failed");
    } else result = true;

    ClosePrinter(hPrinter);
    return result;
}


bool PortInstaller::uninstallMonitor() {
    if (!isInstalled()) return true;

    std::unique_ptr<uint8_t[]> buffer;
    DWORD needed = 0, returned = 0;
    PORT_INFO_2W * pi;
    /* Check if monitor is still in use */
    EnumPorts(NULL, 2, NULL, 0, &needed, &returned);
    if (needed > 0) {
        buffer.reset(new uint8_t[needed]);
        pi = (PORT_INFO_2W *)buffer.get();
        if (EnumPorts(NULL, 2, (LPBYTE)pi, needed, &needed, &returned)) {
            for (DWORD i = 0; i < returned; i++) {
                if (monitorName.isEqualTo(pi[i].pMonitorName)) {
                    deletePort(pi[i].pPortName);
                }
            }
        } else {
            reportError(L"EnumPortsW failed");
            return false;
        }
    } else {
        reportError(L"EnumPortsW failed");
        return false;
    }

    /* Try again, hoping that we succeeded in deleting all ports */
    if (EnumPorts(NULL, 2, (LPBYTE)pi, needed, &needed, &returned)) {
        for (unsigned int i = 0; i < returned; i++) {
            if (monitorName.isEqualTo(pi[i].pMonitorName)) {
                reportError(L"Monitor still in use");
                return false;
            }
        }
    } else {
        reportError(L"EnumPortsW failed");
        return false;
    }

    /* Try to delete the monitor */
    if (DeleteMonitor(NULL, NULL, monitorName)) {
        reportError(L"DeleteMonitor failed");
        return false;
    }

    return true;
}


bool PortInstaller::deletePort(const wchar_t * portName) {
    DWORD needed = 0, returned = 0;
    DWORD flags = PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL;
    EnumPrinters(flags, NULL, 2, NULL, 0, &needed, &returned);
    if (needed > 0) {
        std::unique_ptr<uint8_t[]> enumbuf(new uint8_t[needed]);
        PRINTER_INFO_2W * pri2 = (PRINTER_INFO_2W *)enumbuf.get();
        if (EnumPrinters(flags, NULL, 2, (LPBYTE)pri2, needed, &needed, &returned)) {
            for (unsigned int i = 0; i < returned; i++) {
                if (lstrcmp(pri2[i].pPortName, portName) == 0) {
                    PRINTER_DEFAULTSW pd;
                    pd.pDatatype = NULL;
                    pd.pDevMode = NULL;
                    pd.DesiredAccess = PRINTER_ALL_ACCESS;
                    HANDLE hPrinter;
                    if (OpenPrinter(pri2[i].pPrinterName, &hPrinter, &pd)) {
                        DeletePrinter(hPrinter);
                        ClosePrinter(hPrinter);
                    }
                }
            }
        } else reportError(L"EnumPrinters failed");
    } else reportError(L"EnumPrinters failed");

    /* This may not be silent, but probably is */
    DeletePort(NULL, HWND_DESKTOP, (wchar_t *)portName);
    return true;
}


/**
 * Returns GetLastError()
 * error = Error msg (less than 1024 chars)
 **/
extern "C" DWORD installRedMon(const wchar_t * mon, const wchar_t * dll,
                               const wchar_t * port, wchar_t * error) {
    PortInstaller pi(mon, error);
    if (pi.installMonitor(dll))
        pi.addPort(port);
    return GetLastError();
}


extern "C" DWORD uninstallRedMon(const wchar_t * monitorName, wchar_t * error) {
    PortInstaller pi(monitorName, error);
    pi.uninstallMonitor();
    return GetLastError();
}


extern "C" DWORD renamePrinter(const wchar_t * oldName, const wchar_t * newName,
                               wchar_t * error) {
    PRINTER_DEFAULTSW pd{ NULL, NULL, PRINTER_ALL_ACCESS };
    wsbuffer printerName = oldName;
    HANDLE hPrinter;
    if (OpenPrinter(printerName, &hPrinter, &pd)) {
        DWORD needed = 0;
        GetPrinter(hPrinter, 2, NULL, 0, &needed);
        if (needed > 0) {
            std::unique_ptr<uint8_t[]> pbuf(new uint8_t[needed]);
            PRINTER_INFO_2W * pi = (PRINTER_INFO_2W *)pbuf.get();
            if (GetPrinter(hPrinter, 2, (LPBYTE)pi, needed, &needed)) {
                printerName = newName;
                pi->pPrinterName = printerName;
                SetPrinter(hPrinter, 2, (LPBYTE)pi, 0);
            } else std::wcscpy(error, L"GetPrinter failed");
        } else std::wcscpy(error, L"GetPrinter failed");
        ClosePrinter(hPrinter);
    } else std::wcscpy(error, L"OpenPrinter failed");
    return GetLastError();
}
