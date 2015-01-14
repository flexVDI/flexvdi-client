/* Copyright (C) 1997-2012, Ghostgum Software Pty Ltd.  All rights reserved.

  This file is part of RedMon.

  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  This software is distributed under licence and may not be copied, modified
  or distributed except as expressly authorised under the terms of the
  LICENCE.

*/

/* RedMon uninstall program */
#include <iostream>
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <winspool.h>
#include <shellapi.h>
#include <string>

/* Attempt to disconnect port from all printers, and then delete port.
 * This will only succeed if the redirected port name (e.g. RPT1:)
 * has the same or longer length as FILE:
 */
void disconnect_port(LPTSTR port_name) {
    TCHAR fileport[] = TEXT("FILE:");
    PRINTER_INFO_2 * pri2;
    PRINTER_INFO_2 * printer_info;
    PRINTER_DEFAULTS pd;
    HANDLE hPrinter;
    DWORD needed, returned;
    char enumbuf[65536];
    BYTE printerbuf[65536];
    int rc;
    int len;
    unsigned int i;
    std::wcout << L"Disconnecting port " << port_name << std::endl;
    /* Find printer that uses this port */
    rc = EnumPrinters(PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL,
                      NULL, 2, (LPBYTE)enumbuf, sizeof(enumbuf),
                      &needed, &returned);
    if (rc) {
        pri2 = (PRINTER_INFO_2 *)enumbuf;
        for (i = 0; i < returned; i++) {
            if (lstrcmp(pri2[i].pPortName, port_name) == 0) {
                pd.pDatatype = NULL;
                pd.pDevMode = NULL;
                pd.DesiredAccess = PRINTER_ALL_ACCESS;
                if (OpenPrinter(pri2[i].pPrinterName, &hPrinter, &pd)) {
                    if (GetPrinter(hPrinter, 2, (LPBYTE)printerbuf,
                                   sizeof(printerbuf), &needed)) {
                        printer_info = (PRINTER_INFO_2 *)printerbuf;
                        /* Replace port name with FILE: */
                        len = lstrlen(fileport);
                        if (lstrlen(printer_info->pPortName) >= len) {
                            memcpy(printer_info->pPortName, fileport,
                                   (len + 1) * sizeof(TCHAR));
                        }
                        SetPrinter(hPrinter, 2, printerbuf, 0);
                    }
                    ClosePrinter(hPrinter);
                }
            }
        }
    }
    /* This may not be silent, but probably is */
    DeletePort(NULL, HWND_DESKTOP, port_name);
}


int main(int argc, char * argv[]) {
    char buffer[65536];
    DWORD needed, returned;
    unsigned int i;

    if (argc != 2) return 2;

    auto wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring monitorName(wargv[1]);
    LocalFree(wargv);

    std::wcout << L"Looking for " << monitorName << L"... ";
    /* Check that it really is installed */
    if (EnumMonitors(NULL, 1, (LPBYTE)buffer, sizeof(buffer), &needed, &returned)) {
        MONITOR_INFO_1 * mi = (MONITOR_INFO_1 *)buffer;
        for (i = 0; i < returned; i++) {
            if (mi[i].pName == monitorName) {
                std::cout << "Found" << std::endl;
                break;
            }
        }
        if (i == returned) {
            std::cout << "Not found" << std::endl;
            return 0;
        }
    } else
        return 1;

    /* Check if monitor is still in use */
    if (EnumPorts(NULL, 2, (LPBYTE)buffer, sizeof(buffer), &needed, &returned)) {
        PORT_INFO_2 * pi2 = (PORT_INFO_2 *)buffer;
        for (i = 0; i < returned; i++) {
            if (pi2[i].pMonitorName == monitorName) {
                /* A RedMon port still exists, and may be used by a printer */
                /* Attempt to disconnect and delete port */
                disconnect_port(pi2[i].pPortName);
            }
        }
    } else
        return 1;

    /* Try again, hoping that we succeeded in deleting all ports */
    if (EnumPorts(NULL, 2, (LPBYTE)buffer, sizeof(buffer), &needed, &returned)) {
        PORT_INFO_2 * pi2 = (PORT_INFO_2 *)buffer;
        for (i = 0; i < returned; i++) {
            if (pi2[i].pMonitorName == monitorName) {
                std::wcout << monitorName << L" still in use by port " << pi2[i].pPortName << std::endl;
                /* A RedMon port still exists, and may be used by a printer */
                /* Refuse to uninstall RedMon until the port is deleted */
                return 2;
            }
        }
    } else
        return 1;

    /* Try to delete the monitor */
    return DeleteMonitor(NULL, NULL, (wchar_t *)monitorName.c_str()) ? 0 : 1;
}
