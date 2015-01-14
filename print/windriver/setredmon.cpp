/* Copyright (C) 1997-2012, Ghostgum Software Pty Ltd.  All rights reserved.

  This file is part of RedMon.

  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  This software is distributed under licence and may not be copied, modified
  or distributed except as expressly authorised under the terms of the
  LICENCE.

*/

/* RedMon setup program */
#include <iostream>
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <winspool.h>
#include <shellapi.h>
#include <string>
#include <memory>
#include <system_error>


class WindowsLibrary {
public:
    WindowsLibrary(const wchar_t * name) {
        libHandle = LoadLibraryW(name);
    }
    ~WindowsLibrary() {
        if (libHandle)
            FreeLibrary(libHandle);
    }

    bool isOpen() const {
        return libHandle != nullptr;
    }
    FARPROC getProcAddress(const char * name) const {
        return GetProcAddress(libHandle, name);
    }

private:
    HMODULE libHandle;
};


class No64Redirection {
public:
    typedef BOOL (__stdcall *disableProc)(PVOID *);
    typedef BOOL (__stdcall *revertProc)(PVOID);
    No64Redirection() : oldval(NULL), kernel(L"Kernel32.dll"), revert(nullptr) {
        if (kernel.isOpen()) {
            disableProc fp = (disableProc)kernel.getProcAddress("Wow64DisableWow64FsRedirection");
            if (fp) {
                fp(&oldval);
                revert = (revertProc)kernel.getProcAddress("Wow64RevertWow64FsRedirection");
            }
        }
    }
    ~No64Redirection() {
        if (revert) {
            revert(oldval);
        }
    }

private:
    PVOID oldval;
    WindowsLibrary kernel;
    revertProc revert;
};


class PortInstaller {
public:
    PortInstaller(const wchar_t * mon, const wchar_t * dll, const wchar_t * port)
    : monitorName(mon), monitorDll(dll), portName(port) {}

    bool installMonitor();
    int addPort();
private:
    std::wstring monitorName, monitorDll, portName;
};


bool PortInstaller::installMonitor() {
    /* Check if already installed */
    DWORD needed = 0, returned = 0;
    EnumMonitorsW(NULL, 1, NULL, 0, &needed, &returned);
    if (needed > 0) {
        std::unique_ptr<uint8_t[]> buffer(new uint8_t[needed]);
        MONITOR_INFO_1W * mi = (MONITOR_INFO_1W *)buffer.get();
        if (EnumMonitorsW(NULL, 1, (LPBYTE)mi, needed, &needed, &returned)) {
            for (DWORD i = 0; i < returned; i++) {
                if (lstrcmp(mi[i].pName, monitorName.c_str()) == 0) {
                    return true;
                }
            }
        } else {
            std::cerr << "EnumMonitors failed" << std::endl;
            return false;
        }
    }

    MONITOR_INFO_2W mi2;
    mi2.pName = (wchar_t *)monitorName.c_str();
    mi2.pDLLName = (wchar_t *)monitorDll.c_str();
    mi2.pEnvironment = NULL;
    if (!AddMonitorW(NULL, 2, (LPBYTE)&mi2)) {
        std::cerr << "AddMonitor failed with code " << GetLastError() << " (" <<
            std::system_category().message(GetLastError()).c_str() << ")" << std::endl;
        return false;
    }

    return true;
}


int PortInstaller::addPort() {
    HANDLE hPrinter = INVALID_HANDLE_VALUE;
    PRINTER_DEFAULTS pdef = { NULL, NULL, SERVER_ACCESS_ADMINISTER };
    std::wstring monitorString = L",XcvMonitor " + monitorName;
    BYTE * xcvData = (BYTE *)portName.c_str();
    DWORD xcvSize = (portName.length() + 1) * sizeof(wchar_t);
    DWORD status, needed;
    int result = 1;

    if (!OpenPrinterW((wchar_t *)monitorString.c_str(), &hPrinter, &pdef)) {
        std::cerr << "OpenPrinter failed" << std::endl;
        return 1;
    }

    // MinGw version of XcvDataW seems broken, we load it dynamically
    typedef BOOL (__stdcall *XcvDataWProc)(HANDLE, PCWSTR, PBYTE, DWORD,
                                           PBYTE, DWORD, PDWORD, PDWORD);
    WindowsLibrary spool(L"Winspool.drv");
    if (spool.isOpen()) {
        XcvDataWProc fp = (XcvDataWProc)spool.getProcAddress("XcvDataW");
        if (fp && fp(hPrinter, L"AddPort", xcvData, xcvSize, NULL, 0, &needed, &status)) {
            result = 0;
            if (status != NO_ERROR) {
                std::cerr << "XcvData got status " << status << std::endl;
                result = status;
            }
        } else
            std::cerr << "XcvData failed with code " << GetLastError() << " (" <<
                std::system_category().message(GetLastError()).c_str() << ")" << std::endl;
    } else {
        std::cerr << "Cannot load Winspool.drv" << std::endl;
    }

    ClosePrinter(hPrinter);
    return result;
}


int main(int argc, char * argv[]) {
    No64Redirection no64r;
    auto wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int result = 1;
    if (argc == 4) {
        PortInstaller pi(wargv[1], wargv[2], wargv[3]);
        if (pi.installMonitor()) result = pi.addPort();
    }
    LocalFree(wargv);
    return result;
}
