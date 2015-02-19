/**
 * Copyright Flexible Software Solutions S.L. 2014
 *
 * This file is adapted from RedMon,
 * Copyright (C) 1997-2012, Ghostgum Software Pty Ltd.  All rights reserved.
 *
 * Support for Windows NT <5.0 has been removed.
 */

/* portmon.c */

/*
 * This is a Port Monitor for
 *   Windows 95, 98
 *   Windows NT 3.5
 *   Windows NT 4.0
 *   Windows NT 5.0 (Windows 2000 Professional)
 *
 * This file contains the general port monitor code
 * but not the code that actually does the useful work.
 *
 * For efficiency reasons, don't use the C run time library.
 *
 * The Windows NT version must use Unicode, so Windows NT
 * specific code is conditionally compiled with a combination
 * of UNICODE, NT35, NT40 and NT50.
 */

/* Publicly accessibly functions in this Port Monitor are prefixed with
 * the following:
 *   rXXX   Windows 95 / NT3.5 / NT4
 *   rsXXX  Windows NT5 port monitor server
 *   rcXXX  Windows NT5 port monitor client (UI)
 */

#define STRICT
#include <windows.h>
#include <htmlhelp.h>
#include "redmon.h"

/* These are our only global variables */
TCHAR rekey[MAXSTR];   /* Registry key name for our use */
HINSTANCE hdll;

/* we don't rely on the import library having XcvData,
 * since we may be compiling with VC++ 5.0 */
BOOL WINAPI XcvData(HANDLE hXcv, LPCWSTR pszDataName,
                    PBYTE pInputData, DWORD cbInputData,
                    PBYTE pOutputData, DWORD cbOutputData, PDWORD pcbOutputNeeded,
                    PDWORD pdwStatus);

#define PORTSNAME TEXT("Ports")
#define BACKSLASH TEXT("\\")

#ifdef DEBUG_REDMON
void syslog(LPCTSTR buf) {
    int count;
    CHAR cbuf[1024];
    int UsedDefaultChar;
    DWORD cbWritten;
    HANDLE hfile;
    if (buf == NULL)
        buf = TEXT("(null)");

    if ((hfile = CreateFileA("c:\\temp\\redmon.log", GENERIC_WRITE,
                             0 /* no file sharing */, NULL, OPEN_ALWAYS, 0, NULL))
            != INVALID_HANDLE_VALUE) {
        SetFilePointer(hfile, 0, NULL, FILE_END);
        while (lstrlen(buf)) {
            count = min(lstrlen(buf), sizeof(cbuf));
            WideCharToMultiByte(CP_ACP, 0, buf, count,
                                cbuf, sizeof(cbuf), NULL, &UsedDefaultChar);
            buf += count;
            WriteFile(hfile, cbuf, count, &cbWritten, NULL);
        }
        CloseHandle(hfile);
    }
}

void syserror(DWORD err) {
    LPVOID lpMessageBuffer;
    TCHAR buf[MAXSTR];
    wsprintf(buf, TEXT(" error=%d\r\n"), err);
    syslog(buf);
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL, err,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* user default language */
                  (LPTSTR) &lpMessageBuffer, 0, NULL);
    if (lpMessageBuffer) {
        syslog((LPTSTR)lpMessageBuffer);
        syslog(TEXT("\r\n"));
        LocalFree(LocalHandle(lpMessageBuffer));
    }
}

void syshex(DWORD num) {
    TCHAR buf[MAXSTR];
    wsprintf(buf, TEXT("0x%x"), num);
    syslog(buf);
}

void sysnum(DWORD num) {
    TCHAR buf[MAXSTR];
    wsprintf(buf, TEXT("%d"), num);
    syslog(buf);
}
#endif

LONG RedMonOpenKey(HANDLE hMonitor, LPCTSTR pszSubKey, REGSAM samDesired, PHKEY phkResult) {
    LONG rc = ERROR_SUCCESS;
#ifdef DEBUG_REDMON
    syslog(TEXT("RedMonOpenKey "));
    syslog(pszSubKey);
    syslog(TEXT("\r\n"));
#endif
    if (hMonitor) {
        /* NT50 */
        MONITORINIT * pMonitorInit = (MONITORINIT *)hMonitor;
        rc = pMonitorInit->pMonitorReg->fpOpenKey(
                 pMonitorInit->hckRegistryRoot,
                 pszSubKey,
                 samDesired,
                 (PHANDLE)phkResult,
                 pMonitorInit->hSpooler);
    } else {
        TCHAR buf[MAXSTR];
        lstrcpy(buf, rekey);
        if (pszSubKey) {
            lstrcat(buf, BACKSLASH);
            lstrcat(buf, pszSubKey);
        }
        rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, buf, 0, samDesired,
                          (PHKEY)phkResult);
    }
#ifdef DEBUG_REDMON
    if (rc)
        syserror(rc);
#endif
    return rc;
}

LONG RedMonCloseKey(HANDLE hMonitor, HANDLE hcKey) {
    LONG rc = ERROR_SUCCESS;
#ifdef DEBUG_REDMON
    syslog(TEXT("RedMonCloseKey\r\n"));
#endif
    if (hMonitor) {
        /* NT50 */
        MONITORINIT * pMonitorInit = (MONITORINIT *)hMonitor;
        rc = pMonitorInit->pMonitorReg->fpCloseKey(
                 hcKey, pMonitorInit->hSpooler);
    } else {
        rc = RegCloseKey(hcKey);
    }
#ifdef DEBUG_REDMON
    if (rc)
        syserror(rc);
#endif
    return rc;
}

LONG RedMonEnumKey(HANDLE hMonitor, HANDLE hcKey, DWORD dwIndex,
                   LPTSTR pszName, PDWORD pcchName) {
    LONG rc = ERROR_SUCCESS;
#ifdef DEBUG_REDMON
    syslog(TEXT("RedMonEnumKey "));
    sysnum(dwIndex);
    syslog(TEXT("\r\n"));
#endif
    if (hMonitor) {
        /* NT50 */
        MONITORINIT * pMonitorInit = (MONITORINIT *)hMonitor;
        FILETIME ft;
        rc = pMonitorInit->pMonitorReg->fpEnumKey(
                 hcKey, dwIndex, pszName, pcchName,
                 &ft, pMonitorInit->hSpooler);
    } else {
        rc = RegEnumKey(hcKey, dwIndex, pszName, *pcchName);
    }
#ifdef DEBUG_REDMON
    if (rc)
        syserror(rc);
    else {
        syslog(TEXT(" key="));
        syslog(pszName);
        syslog(TEXT("\r\n"));
    }
#endif
    return rc;
}

LONG RedMonCreateKey(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszSubKey,
                     DWORD dwOptions, REGSAM samDesired,
                     PSECURITY_ATTRIBUTES pSecurityAttributes,
                     PHKEY phckResult, PDWORD pdwDisposition) {
    LONG rc = ERROR_SUCCESS;
#ifdef DEBUG_REDMON
    syslog(TEXT("RedMonCreateKey "));
    syslog(pszSubKey);
    syslog(TEXT("\r\n"));
#endif
    if (hMonitor) {
        /* NT50 */
        MONITORINIT * pMonitorInit = (MONITORINIT *)hMonitor;
        rc = pMonitorInit->pMonitorReg->fpCreateKey(
                 hcKey, pszSubKey, dwOptions,
                 samDesired, pSecurityAttributes, (PHANDLE)phckResult,
                 pdwDisposition, pMonitorInit->hSpooler);
    } else {
        rc = RegCreateKeyEx(hcKey, pszSubKey, 0, 0, dwOptions,
                            samDesired, pSecurityAttributes, (PHKEY)phckResult,
                            pdwDisposition);
    }
#ifdef DEBUG_REDMON
    if (rc)
        syserror(rc);
#endif
    return rc;
}

LONG RedMonDeleteKey(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszSubKey) {
    LONG rc = ERROR_SUCCESS;
#ifdef DEBUG_REDMON
    syslog(TEXT("RedMonDeleteKey "));
    syslog(pszSubKey);
    syslog(TEXT("\r\n"));
#endif
    if (hMonitor) {
        /* NT50 */
        MONITORINIT * pMonitorInit = (MONITORINIT *)hMonitor;
        rc = pMonitorInit->pMonitorReg->fpDeleteKey(
                 hcKey, pszSubKey, pMonitorInit->hSpooler);
    } else {
        rc = RegDeleteKey(hcKey, pszSubKey);
    }
#ifdef DEBUG_REDMON
    if (rc)
        syserror(rc);
#endif
    return rc;
}


LONG RedMonSetValue(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszValue, DWORD dwType,
                    const BYTE * pData, DWORD cbData) {
    LONG rc = ERROR_SUCCESS;
#ifdef DEBUG_REDMON
    TCHAR buf[MAXSTR];
    syslog(TEXT("RedMonSetValue "));
    syslog(pszValue);
    if (dwType == REG_DWORD) {
        wsprintf(buf, TEXT(" REG_DWORD %ld"), (*(LPDWORD)pData));
        syslog(buf);
    } else
        if (dwType = REG_SZ) {
            wsprintf(buf, TEXT(" REG_SZ \042%s\042"), pData);
            syslog(buf);
        }
    syslog(TEXT("\r\n"));
#endif
    if (hMonitor) {
        /* NT50 */
        MONITORINIT * pMonitorInit = (MONITORINIT *)hMonitor;
        rc = pMonitorInit->pMonitorReg->fpSetValue(hcKey, pszValue,
                dwType, pData, cbData, pMonitorInit->hSpooler);
    } else {
        rc = RegSetValueEx(hcKey, pszValue, 0, dwType,
                           pData, cbData);
    }
#ifdef DEBUG_REDMON
    if (rc)
        syserror(rc);
#endif
    return rc;
}


LONG RedMonQueryValue(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszValue,
                      PDWORD pType, PBYTE pData, PDWORD pcbData) {
    LONG rc = ERROR_SUCCESS;
#ifdef DEBUG_REDMON
    syslog(TEXT("RedMonQueryValue "));
    syslog(pszValue);
    syslog(TEXT("\r\n"));
#endif
    if (hMonitor) {
        /* NT50 */
        MONITORINIT * pMonitorInit = (MONITORINIT *)hMonitor;
        rc = pMonitorInit->pMonitorReg->fpQueryValue(
                 hcKey, pszValue, pType, pData, pcbData,
                 pMonitorInit->hSpooler);
    } else {
        rc = RegQueryValueEx(hcKey, pszValue, 0, pType,
                             pData, pcbData);
    }
#ifdef DEBUG_REDMON
    if (rc)
        syserror(rc);
#endif
    return rc;
}


BOOL WINAPI rsEnumPorts(HANDLE hMonitor, LPTSTR pName, DWORD Level,
                        LPBYTE pPorts, DWORD cbBuf, LPDWORD pcbNeeded, LPDWORD pcReturned) {
    HKEY hkey, hsubkey;
    TCHAR portname[MAXSHORTSTR], portdesc[MAXSHORTSTR];
    TCHAR monitorname[MAXSHORTSTR];
    TCHAR buf[MAXSTR];
    int needed;
    DWORD cbData, keytype;
    PORT_INFO_1 * pi1;
    PORT_INFO_2 * pi2;
    LPTSTR pstr;
    LONG rc;
    int i;

#ifdef DEBUG_REDMON
    syslog(TEXT("rsEnumPorts\r\n"));
    {
        TCHAR buf[MAXSTR];
        wsprintf(buf, TEXT("  hMonitor=0x%0x\r\n"), hMonitor);
        syslog(buf);
    }
    syslog(TEXT("  pName="));
    sysnum((DWORD)pName);
    syslog(TEXT("\r\n"));
#endif

    *pcbNeeded = 0;
    *pcReturned = 0;

    if (hMonitor == NULL) {
        if (pName != NULL)
            return FALSE;
    }

    if ((Level < 1) || (Level > 2)) {
        SetLastError(ERROR_INVALID_LEVEL);
        return FALSE;
    }

    rc = RedMonOpenKey(hMonitor, PORTSNAME, KEY_READ, &hkey);
    if (rc != ERROR_SUCCESS)
        return TRUE;    /* There are no ports */

    lstrcpyn(monitorname, MONITORNAME, sizeof(monitorname) / sizeof(TCHAR) - 1);

    /* First pass is to calculate the number of bytes needed */
    needed = 0;
    i = 0;
    cbData = sizeof(portname);
    rc = RedMonEnumKey(hMonitor, hkey, 0, portname, &cbData);
    while (rc == ERROR_SUCCESS) {
        needed += (lstrlen(portname) + 1) * sizeof(TCHAR);
        if (Level == 1) {
            needed += sizeof(PORT_INFO_1);
        } else
            if (Level == 2) {
                needed += sizeof(PORT_INFO_2);
                needed += (lstrlen(monitorname) + 1) * sizeof(TCHAR);
                lstrcpy(buf, PORTSNAME);
                lstrcat(buf, BACKSLASH);
                lstrcat(buf, portname);
                rc = RedMonOpenKey(hMonitor, buf, KEY_READ, &hsubkey);
                if (rc == ERROR_SUCCESS) {
                    cbData = sizeof(portdesc);
                    keytype = REG_SZ;
                    RedMonQueryValue(hMonitor, hsubkey, DESCKEY, &keytype,
                                     (LPBYTE)portdesc, &cbData);
                    if (rc == ERROR_SUCCESS)
                        needed += (lstrlen(portdesc) + 1) * sizeof(TCHAR);
                    else
                        needed += 1 * sizeof(TCHAR);    /* empty string */
                    RedMonCloseKey(hMonitor, hsubkey);
                } else {
                    needed += 1 * sizeof(TCHAR);    /* empty string */
                }
            }
        i++;
        cbData = sizeof(portname);
        rc = RedMonEnumKey(hMonitor, hkey, i, portname, &cbData);
    }
    *pcbNeeded = needed;

    if ((pPorts == NULL) || ((unsigned int)needed > cbBuf)) {
        RedMonCloseKey(hMonitor, hkey);
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    /* Second pass to copy the data to the buffer */

    /* PORT_INFO_x structures must be placed at the beginning
     * of the buffer, and strings at the end of the buffer.
     * This is important!  It appears that one buffer is
     * allocated, then each port monitor is called in turn to
     * add its entries to the buffer, between previous entries.
     */

    i = 0;
    pi1 = (PORT_INFO_1 *)pPorts;
    pi2 = (PORT_INFO_2 *)pPorts;
    pstr = (LPTSTR)(pPorts + cbBuf);
    cbData = sizeof(portname);
    rc = RedMonEnumKey(hMonitor, hkey, 0, portname, &cbData);
    while (rc == ERROR_SUCCESS) {
        if (Level == 1) {
            pstr -= lstrlen(portname) + 1;
            lstrcpy(pstr, portname);
            pi1[i].pName = pstr;
        } else
            if (Level == 2) {
                pstr -= lstrlen(portname) + 1;
                lstrcpy(pstr, portname);
                pi2[i].pPortName = pstr;

                pstr -= lstrlen(monitorname) + 1;
                lstrcpy(pstr, monitorname);
                pi2[i].pMonitorName = pstr;

                lstrcpy(buf, PORTSNAME);
                lstrcat(buf, BACKSLASH);
                lstrcat(buf, portname);
                rc = RedMonOpenKey(hMonitor, buf, KEY_READ, &hsubkey);
                if (rc == ERROR_SUCCESS) {
                    cbData = sizeof(portdesc);
                    keytype = REG_SZ;
                    RedMonQueryValue(hMonitor, hsubkey, DESCKEY, &keytype,
                                     (LPBYTE)portdesc, &cbData);
                    if (rc != ERROR_SUCCESS)
                        portdesc[0] = '\0';
                    RedMonCloseKey(hMonitor, hsubkey);
                } else {
                    portdesc[0] = '\0';
                }

                pstr -= lstrlen(portdesc) + 1;
                lstrcpy(pstr, portdesc);
                pi2[i].pDescription = pstr;

                /* Say that writing to this port is supported, */
                /* but reading is not. */
                /* Using fPortType = 3 is wrong. */
                /* Options are PORT_TYPE_WRITE=1, PORT_TYPE_READ=2, */
                /*   PORT_TYPE_REDIRECTED=4, PORT_TYPE_NET_ATTACHED=8 */
                pi2[i].fPortType = PORT_TYPE_WRITE;
                pi2[i].Reserved = 0;
            }
        i++;
        cbData = sizeof(portname);
        rc = RedMonEnumKey(hMonitor, hkey, i, portname, &cbData);
    }
    *pcReturned = i;
    RedMonCloseKey(hMonitor, hkey);

    return TRUE;
}


typedef BOOL (WINAPI * pfnRealEnumPorts)
(LPTSTR pName, DWORD Level, LPBYTE pPorts,
 DWORD cbBuf, LPDWORD pcbNeeded, LPDWORD pcReturned);

BOOL sPortExists(HANDLE hMonitor, LPTSTR pszPortName) {
    BOOL port_exists = FALSE;
    HGLOBAL hglobal;
    LPBYTE ports;
    DWORD cbBuf, needed, returned;
    PORT_INFO_2 * pi2;
    int j;
    pfnRealEnumPorts RealEnumPorts;

#ifdef DEBUG_REDMON
    syslog(TEXT("sPortExists "));
    syslog(pszPortName);
    syslog(TEXT("\r\n"));
    {
        TCHAR buf[MAXSTR];
        wsprintf(buf, TEXT("  hMonitor=0x%0x\r\n"), hMonitor);
        syslog(buf);
    }
#endif
    /* Get the list of existing ports. */
    /* Ask for all ports, not just ours.
     * If we ask for only RedMon ports by calling rsEnumPorts, a user
     * can create a second LPT1: This isn't useful because it doesn't
     * get serviced before the Local Port, and we can't delete it
     * with the Delete Port button.  Instead we have to use the
     * Registry Editor to clean up the mess.
     * To avoid this, stop users from entering any existing port name.
     */
    RealEnumPorts = EnumPorts;

    needed = 0;
    /* 1998-01-08
     * The TELES.FaxMon monitor is buggy - it returns an incorrect
     * value for "needed" if we use the following call.
     *    EnumPorts(NULL, 2, NULL, 0, &needed, &returned);
     * To dodge this bug, pass it buffer of non-zero size.
     */

    cbBuf = 4096;
    hglobal = GlobalAlloc(GPTR, (DWORD)cbBuf);
    ports = GlobalLock(hglobal);
    if (ports == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    if (!RealEnumPorts(NULL, 2, ports, cbBuf, &needed, &returned)) {
#ifdef DEBUG_REDMON
        DWORD err = GetLastError();
        syslog(TEXT("EnumPorts failed\r\n"));
        syserror(err);
#endif
        GlobalUnlock(hglobal);
        GlobalFree(hglobal);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return FALSE;   /* give up */
        } else {
            /* try again, with requested size */
            cbBuf = needed;
            /* If there are no ports installed, avoid trying to
            * allocate 0 bytes
            */
            hglobal = GlobalAlloc(GPTR, (DWORD)cbBuf + 4);
            ports = GlobalLock(hglobal);
            if (ports == NULL) {
                SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                return FALSE;
            }
            if (!RealEnumPorts(NULL, 2, ports, cbBuf, &needed, &returned)) {
#ifdef DEBUG_REDMON
                DWORD err = GetLastError();
                syslog(TEXT("EnumPorts failed\r\n"));
                syserror(err);
#endif
                GlobalUnlock(hglobal);
                GlobalFree(hglobal);
                return FALSE;
                return FALSE;
            }
        }
    }
#ifdef DEBUG_REDMON
    syslog(TEXT("EnumPorts returns "));
    sysnum(returned);
    syslog(TEXT(" port \r\n"));
#endif

    pi2 = (PORT_INFO_2 *)ports;

    for (j = 0; j < (int)returned; j++) {
#ifdef DEBUG_REDMON
        syslog(TEXT("  port "));
        sysnum(j);
        syslog(TEXT("='"));
        syslog(pi2[j].pPortName);
        syslog(TEXT("'\r\n"));
#endif
        if (lstrcmp(pszPortName, pi2[j].pPortName) == 0)
            port_exists = TRUE;
    }

    GlobalUnlock(hglobal);
    GlobalFree(hglobal);

#ifdef DEBUG_REDMON
    syslog(TEXT("sPortExists returns "));
    syslog(port_exists ? TEXT("true\r\n") : TEXT("false\r\n"));
#endif

    return port_exists;
}


TCHAR XcvMonitor[] = TEXT("XcvMonitor");
TCHAR XcvPort[] = TEXT("XcvPort");

/* Copy to pServerName the printer name for accessing
 * the port monitor server */
BOOL MakeXcvName(LPTSTR pServerName, LPCTSTR pServer, LPCTSTR pType,
                 LPCTSTR pName) {
    DWORD len = 2 + lstrlen(pServer) + 2 + lstrlen(pType) + 1 +
                lstrlen(pName) + 1;
    if (len > MAXSTR)
        return FALSE;

    *pServerName = 0;
    if (pServer) {
        lstrcat(pServerName, pServer);
        lstrcat(pServerName, TEXT("\\"));
    }
    lstrcat(pServerName, TEXT(","));
    lstrcat(pServerName, pType);
    lstrcat(pServerName, TEXT(" "));
    if (pName)
        lstrcat(pServerName, pName);
    return TRUE;
}

DWORD cConfigurePortUI(HANDLE hXcv, HWND hWnd, PCWSTR pszPortName) {
#ifdef DEBUG_REDMON
    syslog(TEXT("cConfigurePortUI\r\n"));
#endif
    return ERROR_SUCCESS;
}

BOOL WINAPI rcConfigurePortUI(PCWSTR pszServer, HWND hWnd, PCWSTR pszPortName) {
    TCHAR pszServerName[MAXSTR];
    HANDLE hXcv = NULL;
    DWORD dwError;
    PRINTER_DEFAULTS pd;

    if (!MakeXcvName(pszServerName, pszServer, XcvPort, pszPortName))
        return FALSE;

    pd.pDatatype = NULL;
    pd.pDevMode = NULL;
    pd.DesiredAccess = SERVER_ACCESS_ADMINISTER;

#ifdef DEBUG_REDMON
    syslog(TEXT("OpenPrinter "));
    syslog(pszServerName);
    syslog(TEXT("\r\n"));
#endif
    if (!OpenPrinter(pszServerName, &hXcv, &pd))
        return FALSE;

    dwError = cConfigurePortUI(hXcv, hWnd, pszPortName);

    ClosePrinter(hXcv);

    return (dwError == ERROR_SUCCESS);
}

BOOL sAddPort(HANDLE hMonitor, LPTSTR pszPortName) {
    LONG rc;
    HKEY hkey, hsubkey;
    TCHAR szPortDesc[MAXSHORTSTR];
#ifdef DEBUG_REDMON
    syslog(TEXT("sAddPort\r\n"));
#endif

    if (sPortExists(hMonitor, pszPortName))
        return FALSE;

    /* store new port name in registry */
    lstrcpyn(szPortDesc, MONITORNAME, sizeof(szPortDesc) / sizeof(TCHAR) - 1);
    rc = RedMonOpenKey(hMonitor, PORTSNAME, KEY_ALL_ACCESS, &hkey);
    if (rc != ERROR_SUCCESS) {
        /* try to create the Ports key */
        rc = RedMonOpenKey(hMonitor, NULL, KEY_ALL_ACCESS, &hkey);
        if (rc == ERROR_SUCCESS) {
            rc = RedMonCreateKey(hMonitor, hkey, PORTSNAME, 0,
                                 KEY_ALL_ACCESS, NULL, &hsubkey, NULL);
            if (rc == ERROR_SUCCESS)
                RedMonCloseKey(hMonitor, hsubkey);
            RedMonCloseKey(hMonitor, hkey);
        }
        rc = RedMonOpenKey(hMonitor, PORTSNAME, KEY_ALL_ACCESS, &hkey);
    }
    if (rc == ERROR_SUCCESS) {
        rc = RedMonCreateKey(hMonitor, hkey, pszPortName, 0, KEY_ALL_ACCESS,
                             NULL, &hsubkey, NULL);
        if (rc == ERROR_SUCCESS) {
            rc = RedMonSetValue(hMonitor, hsubkey, DESCKEY, REG_SZ,
                                (CONST BYTE *)szPortDesc, (lstrlen(szPortDesc) + 1) * sizeof(TCHAR));
            RedMonCloseKey(hMonitor, hsubkey);
        }
        RedMonCloseKey(hMonitor, hkey);
    }
    return (rc == ERROR_SUCCESS);
}


BOOL WINAPI rcAddPortUI(PCWSTR pszServer, HWND hWnd,
                        PCWSTR pszPortNameIn, PWSTR * ppszPortNameOut) {
    return FALSE;
}


BOOL sDeletePort(HANDLE hMonitor, LPCTSTR pPortName) {
    HKEY hkey;
    LONG rc;
#ifdef DEBUG_REDMON
    syslog(TEXT("sDeletePort "));
    syslog(pPortName);
    syslog(TEXT("\r\n"));
#endif
    rc = RedMonOpenKey(hMonitor, PORTSNAME, KEY_ALL_ACCESS, &hkey);
#ifdef DEBUG_REDMON
    syslog(TEXT(" hMonitor="));
    syshex((DWORD)hMonitor);
    syslog(TEXT("\r\n"));
    syslog(TEXT(" hKey="));
    syshex((DWORD)hkey);
    syslog(TEXT("\r\n"));
    syslog(TEXT(" pPortName="));
    syshex((DWORD)pPortName);
    syslog(TEXT(" \042"));
    syslog(pPortName);
    syslog(TEXT("\042\r\n"));
#endif
    if (rc == ERROR_SUCCESS) {
        rc = RedMonDeleteKey(hMonitor, hkey, pPortName);
        RedMonCloseKey(hMonitor, hkey);
    }
    return (rc == ERROR_SUCCESS);
}

BOOL WINAPI rcDeletePortUI(PCWSTR pszServer, HWND hWnd, PCWSTR pszPortName) {
    TCHAR pszServerName[MAXSTR];
    HANDLE hXcv = NULL;
    DWORD dwStatus = ERROR_SUCCESS;
    DWORD dwError = ERROR_SUCCESS;
    DWORD dwOutput = 0;
    DWORD dwNeeded = 0;
    PRINTER_DEFAULTS pd;

#ifdef DEBUG_REDMON
    syslog(TEXT("rcDeletePortUI "));
    syslog(pszPortName);
    syslog(TEXT("\r\n"));
#endif

    if (!MakeXcvName(pszServerName, pszServer, XcvPort, pszPortName)) {
#ifdef DEBUG_REDMON
        syslog(TEXT("MakeXcvName failed\r\n"));
#endif
        return FALSE;
    }

    pd.pDatatype = NULL;
    pd.pDevMode = NULL;
    pd.DesiredAccess = SERVER_ACCESS_ADMINISTER;

#ifdef DEBUG_REDMON
    syslog(TEXT("OpenPrinter "));
    syslog(pszServerName);
    syslog(TEXT("\r\n"));
#endif

    if (!OpenPrinter(pszServerName, &hXcv, &pd)) {
#ifdef DEBUG_REDMON
        dwError = GetLastError();
        syslog(TEXT("OpenPrinter failed\r\n"));
        syserror(dwError);
#endif
        return FALSE;
    }

#ifdef DEBUG_REDMON
    {
        TCHAR buf[MAXSTR];
        wsprintf(buf, TEXT("rcDeletePortUI hXcv=0x%x\r\n"), hXcv);
        syslog(buf);
    }
#endif

    /* Delete port */
    if (!XcvData(hXcv, TEXT("DeletePort"), (PBYTE)pszPortName,
                 sizeof(TCHAR) * (lstrlen(pszPortName) + 1),
                 (PBYTE)(&dwOutput), 0, &dwNeeded, &dwStatus)) {
#ifdef DEBUG_REDMON
        dwError = GetLastError();
        syslog(TEXT("XcvData failed\r\n"));
        syserror(dwError);
#endif
    }

#ifdef DEBUG_REDMON
    syslog(TEXT("XcvData DeletePort returns "));
    sysnum(dwStatus);
    syslog(TEXT("\r\n"));
    if (dwStatus)
        syserror(dwStatus);
#endif

    ClosePrinter(hXcv);

    return (dwStatus == ERROR_SUCCESS);
}


typedef struct tagREXCVPORT {
    DWORD dwSize;
    HANDLE hMonitor;
    TCHAR szPortName[MAXSTR];
    ACCESS_MASK GrantedAccess;
} REXCVPORT;

BOOL WINAPI rsXcvOpenPort(HANDLE hMonitor, LPCWSTR pszObject,
                          ACCESS_MASK GrantedAccess, PHANDLE phXcv) {
    REXCVPORT * xcv;
#ifdef DEBUG_REDMON
    syslog(TEXT("rsXcvOpenPort\r\n"));
#endif
    *phXcv = (PHANDLE)NULL;
    xcv = GlobalLock(GlobalAlloc(GPTR, (DWORD)sizeof(REXCVPORT)));
    if (xcv == NULL)
        return FALSE;

    memset(xcv, 0, sizeof(REXCVPORT));
    xcv->dwSize = sizeof(REXCVPORT);
    xcv->hMonitor = hMonitor;
    lstrcpyn(xcv->szPortName, pszObject,
             sizeof(xcv->szPortName) / sizeof(TCHAR) - 1);
    xcv->GrantedAccess = GrantedAccess;
#ifdef DEBUG_REDMON
    {
        TCHAR buf[MAXSTR];
        wsprintf(buf, TEXT("rsXcvOpenPort hXcv=0x%x\r\n"), xcv);
        wsprintf(buf, TEXT(" hMonitor = 0x%x\r\n"), xcv->hMonitor);
        syslog(buf);
    }
#endif
    *phXcv = (PHANDLE)xcv;
    return TRUE;
}


DWORD WINAPI rsXcvDataPort(HANDLE hXcv, LPCWSTR pszDataName, PBYTE pInputData,
                           DWORD cbInputData, PBYTE pOutputData, DWORD cbOutputData,
                           PDWORD pcbOutputNeeded) {
    REXCVPORT * xcv = (REXCVPORT *)hXcv;

#ifdef DEBUG_REDMON
    syslog(TEXT("rsXcvDataPort\r\n  "));
    syslog(pszDataName);
    syslog(TEXT("\r\n"));
    syshex((DWORD)hXcv);
    syslog(TEXT("\r\n"));
#endif

    if ((xcv == NULL) || (xcv->dwSize != sizeof(REXCVPORT))) {
#ifdef DEBUG_REDMON
        syslog(TEXT("Invalid parameter\r\n"));
#endif
        return ERROR_INVALID_PARAMETER;
    }

    if (lstrcmp(pszDataName, TEXT("AddPort")) == 0) {
        if (!(xcv->GrantedAccess & SERVER_ACCESS_ADMINISTER))
            return ERROR_ACCESS_DENIED;
        /* pInputData contains the port name */
        if (sAddPort(xcv->hMonitor, (LPTSTR)pInputData))
            return ERROR_SUCCESS;
    } else
        if (lstrcmp(pszDataName, TEXT("DeletePort")) == 0) {
            if (!(xcv->GrantedAccess & SERVER_ACCESS_ADMINISTER))
                return ERROR_ACCESS_DENIED;
            if (sDeletePort(xcv->hMonitor, (LPTSTR)pInputData))
                return ERROR_SUCCESS;
        } else
            if (lstrcmp(pszDataName, TEXT("MonitorUI")) == 0) {
                /* Our server and UI DLLs are combined */
                TCHAR buf[MAXSTR];
                DWORD len;
                if (pOutputData == NULL)
                    return ERROR_INSUFFICIENT_BUFFER;
                len = GetModuleFileName(hdll, buf, sizeof(buf) / sizeof(TCHAR) - 1);
#ifdef DEBUG_REDMON
                {
                    TCHAR mess[MAXSTR];
                    wsprintf(mess, TEXT("GetModuleFileName=\042%s\042 len=%d\r\n"),
                             buf, len);
                    syslog(mess);
                }
#endif
                *pcbOutputNeeded = (len + 1) * sizeof(TCHAR);
                if (*pcbOutputNeeded > cbOutputData)
                    return ERROR_INSUFFICIENT_BUFFER;
                memcpy(pOutputData, buf, *pcbOutputNeeded);
#ifdef DEBUG_REDMON
                syslog(TEXT(" returned UI name="));
                syslog((LPTSTR)pOutputData);
                syslog(TEXT("\r\n"));
#endif
                return ERROR_SUCCESS;
            } else
                if (lstrcmp(pszDataName, TEXT("PortExists")) == 0) {
                    /* pInputData contains the port name */
                    if (sPortExists(xcv->hMonitor, (LPTSTR)pInputData))
                        return ERROR_PRINTER_ALREADY_EXISTS;    /* TRUE */
                    else
                        return ERROR_SUCCESS;           /* FALSE */
                } else
                    if (lstrcmp(pszDataName, TEXT("GetConfig")) == 0) {
                        *pcbOutputNeeded = redmon_sizeof_config();
                        if (*pcbOutputNeeded > cbOutputData)
                            return ERROR_INSUFFICIENT_BUFFER;
                        if (redmon_get_config(xcv->hMonitor, (LPCTSTR)pInputData,
                                              (RECONFIG *)pOutputData))
                            return ERROR_SUCCESS;
                    } else
                        if (lstrcmp(pszDataName, TEXT("SetConfig")) == 0) {
                            if (!(xcv->GrantedAccess & SERVER_ACCESS_ADMINISTER))
                                return ERROR_ACCESS_DENIED;
                            if (!redmon_validate_config((RECONFIG *)pInputData))
                                return ERROR_INVALID_PARAMETER;
                            if (redmon_set_config(xcv->hMonitor, (RECONFIG *)pInputData))
                                return ERROR_SUCCESS;
                        }

    return ERROR_INVALID_PARAMETER;
}

BOOL WINAPI rsXcvClosePort(HANDLE hXcv) {
    REXCVPORT * xcv = (REXCVPORT *)hXcv;
#ifdef DEBUG_REDMON
    syslog(TEXT("rsXcvClosePort\r\n"));
#endif
    if ((xcv == NULL) || (xcv->dwSize != sizeof(REXCVPORT)))
        return FALSE;
    GlobalUnlock(GlobalHandle(hXcv));
    GlobalFree(GlobalHandle(hXcv));
    return TRUE;
}

VOID WINAPI rsShutdown(HANDLE hMonitor) {
    /* undo InitializePrintMonitor2 */
    /* nothing to do */
#ifdef DEBUG_REDMON
    syslog(TEXT("rsShutDown\r\n"));
#endif
}


/* We don't have XcvData in the import library, so we must use
 * run time linking.  This is inefficient, but fortunately we
 * only need this for port configuration, not for normal use.
 * Using run time linking also allows to have one DLL which
 * works for both Windows NT4 and 2000. */
typedef BOOL (WINAPI * pfnXcvData)(HANDLE hXcv, LPCWSTR pszDataName,
                                   PBYTE pInputData, DWORD cbInputData,
                                   PBYTE pOutputData, DWORD cbOutputData, PDWORD pcbOutputNeeded,
                                   PDWORD pdwStatus);

BOOL WINAPI XcvData(HANDLE hXcv, LPCWSTR pszDataName,
                    PBYTE pInputData, DWORD cbInputData,
                    PBYTE pOutputData, DWORD cbOutputData, PDWORD pcbOutputNeeded,
                    PDWORD pdwStatus) {
    pfnXcvData fpXcvData = NULL;
    BOOL flag = FALSE;
    /* Load the spooler DLL.  This shouldn't be too slow, since
     * the DLL should already be loaded */
    HINSTANCE hInstance = LoadLibrary(TEXT("winspool.drv"));
#ifdef DEBUG_REDMON
    syslog(TEXT("loader for XcvData "));
    syslog(pszDataName);
    syslog(TEXT("\r\n"));
#endif
    if (hInstance != NULL) {
        fpXcvData = (pfnXcvData)GetProcAddress(hInstance, "XcvDataW");
        if (fpXcvData != NULL) {
            flag = fpXcvData(hXcv, pszDataName, pInputData, cbInputData,
                             pOutputData, cbOutputData, pcbOutputNeeded, pdwStatus);
#ifdef DEBUG_REDMON
            {
                TCHAR buf[MAXSTR];
                DWORD dwError = GetLastError();
                wsprintf(buf,
                         TEXT("XcvData returns %d, dwStatus=%d, GetLastError=%d\r\n"),
                         flag, *pdwStatus, dwError);
                syslog(buf);
                if (flag == 0)
                    syserror(dwError);
                wsprintf(buf, TEXT("hXcv=0x%x\r\n"), hXcv);
                syslog(buf);
            }
#endif
        }
#ifdef DEBUG_REDMON
        else {
            syslog(TEXT("failed to load XcvData\r\n"));
        }
#endif
        FreeLibrary(hInstance);
    }
    return flag;
}


BOOL WINAPI rsOpenPort(HANDLE hMonitor, LPTSTR pName, PHANDLE pHandle) {
    BOOL flag;

#ifdef DEBUG_REDMON
    syslog(TEXT("rsOpenPort"));
    syslog(TEXT("  hMonitor="));
    syshex((DWORD)hMonitor);
    syslog(TEXT("\r\n"));
    syslog(TEXT("  pName="));
    syslog(pName);
    syslog(TEXT("\r\n"));
    syslog(TEXT("\r\n"));
#endif

    flag = redmon_open_port(hMonitor, pName, pHandle);

#ifdef DEBUG_REDMON
    syslog(TEXT("rsOpenPort returns "));
    sysnum(flag);
    syslog(TEXT("\r\n"));
#endif
    return flag;
}


/* 95 / NT 40 / NT 50 */
BOOL WINAPI rStartDocPort(HANDLE hPort, LPTSTR pPrinterName,
                          DWORD JobId, DWORD Level, LPBYTE pDocInfo) {
    REDATA * prd = GlobalLock((HGLOBAL)hPort);
    BOOL flag;

#ifdef DEBUG_REDMON
    syslog(TEXT("rStartDocPort\r\n"));
    syslog(TEXT("  hPort="));
    syshex((DWORD)hPort);
    syslog(TEXT("\r\n"));
#endif

    if (prd == (REDATA *)NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    reset_redata(prd);

    flag = redmon_start_doc_port(prd, pPrinterName, JobId, Level, pDocInfo);

    GlobalUnlock((HGLOBAL)hPort);

#ifdef DEBUG_REDMON
    syslog(TEXT("rStartDocPort returns "));
    sysnum(flag);
    syslog(TEXT("\r\n"));
#endif
    return flag;
}


/* WritePort is normally called between StartDocPort and EndDocPort,
 * but can be called outside this pair for bidirectional printers.
 */
BOOL WINAPI rWritePort(HANDLE  hPort, LPBYTE  pBuffer,
                       DWORD   cbBuf, LPDWORD pcbWritten) {
    REDATA * prd = GlobalLock((HGLOBAL)hPort);
    BOOL flag;
#ifdef DEBUG_REDMON
    syslog(TEXT("rWritePort "));
    sysnum(cbBuf);
    syslog(TEXT("\r\n"));
#endif

    if (prd == (REDATA *)NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    *pcbWritten = 0;
    flag = redmon_write_port(prd, pBuffer, cbBuf, pcbWritten);

    GlobalUnlock((HGLOBAL)hPort);

#ifdef DEBUG_REDMON
    syslog(TEXT("rWritePort returns TRUE\r\n"));
#endif
    return TRUE;    /* returning FALSE crashes Win95 spooler */
}

/* ReadPort can be called within a Start/EndDocPort pair,
 * and also outside this pair.
 */
BOOL WINAPI rReadPort(HANDLE hPort, LPBYTE pBuffer,
                      DWORD  cbBuffer, LPDWORD pcbRead) {
    BOOL flag;
    REDATA * prd = GlobalLock((HGLOBAL)hPort);
#ifdef DEBUG_REDMON
    syslog(TEXT("rReadPort\r\n"));
#endif

    /* we don't support reading */
    *pcbRead = 0;

    if (prd == (REDATA *)NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
#ifdef DEBUG_REDMON
        syslog(TEXT("rReadPort returns FALSE.  Not our port handle.\r\n"));
#endif
        return FALSE;
    }

    flag = redmon_read_port(prd, pBuffer, cbBuffer, pcbRead);

    GlobalUnlock((HGLOBAL)hPort);

#ifdef DEBUG_REDMON
    syslog(TEXT("rReadPort returns "));
    sysnum(flag);
    syslog(TEXT("\r\n"));
#endif
    return flag;
}

BOOL WINAPI rEndDocPort(HANDLE hPort) {
    REDATA * prd = GlobalLock((HGLOBAL)hPort);
    BOOL flag;
#ifdef DEBUG_REDMON
    syslog(TEXT("rEndDocPort\r\n"));
#endif

    if (prd == (REDATA *)NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    flag = redmon_end_doc_port(prd);

    reset_redata(prd);

    GlobalUnlock((HGLOBAL)hPort);

#ifdef DEBUG_REDMON
    syslog(TEXT("rEndDocPort returns TRUE\r\n"));
#endif
    return TRUE;
}


BOOL WINAPI rClosePort(HANDLE hPort) {
    BOOL flag;
#ifdef DEBUG_REDMON
    syslog(TEXT("rClosePort\r\n"));
#endif
    flag = redmon_close_port(hPort);
#ifdef DEBUG_REDMON
    syslog(TEXT("rClosePort returns "));
    sysnum(flag);
    syslog(TEXT("\r\n"));
#endif
    return flag;
}


BOOL WINAPI rSetPortTimeOuts(HANDLE  hPort, LPCOMMTIMEOUTS lpCTO, DWORD reserved) {
    REDATA * prd = GlobalLock((HGLOBAL)hPort);
    BOOL flag;
#ifdef DEBUG_REDMON
    syslog(TEXT("rSetPortTimeouts\r\n"));
#endif
    if (prd == (REDATA *)NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    flag = redmon_set_port_timeouts(prd, lpCTO, reserved);

    GlobalUnlock((HGLOBAL)hPort);

#ifdef DEBUG_REDMON
    syslog(TEXT("rSetPortTimeouts returns "));
    sysnum(flag);
    syslog(TEXT("\r\n"));
#endif
    return flag;
}

/* Exported functions */

/* Windows 2000 version */

MONITOR2 mon2 = {
    sizeof(MONITOR2),
    rsEnumPorts,
    rsOpenPort,
    NULL, /* OpenPortEx */
    rStartDocPort,
    rWritePort,
    rReadPort,
    rEndDocPort,
    rClosePort,
    NULL, /* AddPort */
    NULL, /* AddPortEx */
    NULL, /* ConfigurePort */
    NULL, /* DeletePort */
    NULL, /* GetPrinterDataFromPort */
    rSetPortTimeOuts,
    rsXcvOpenPort,
    rsXcvDataPort,
    rsXcvClosePort,
    rsShutdown
};

PORTMONEXPORT LPMONITOR2 WINAPI _export
InitializePrintMonitor2(PMONITORINIT pMonitorInit, PHANDLE phMonitor) {
#ifdef DEBUG_REDMON
    syslog(TEXT("InitializePrintMonitor2"));
    syslog(TEXT("\r\n  cbSize="));
    sysnum(pMonitorInit->cbSize);
    syslog(TEXT("\r\n  hSpooler="));
    syshex((DWORD)(pMonitorInit->hSpooler));
    syslog(TEXT("\r\n  hckRegistryRoot="));
    syshex((DWORD)(pMonitorInit->hckRegistryRoot));
    syslog(TEXT("\r\n  pMonitorReg="));
    syshex((DWORD)(pMonitorInit->pMonitorReg));
    syslog(TEXT("\r\n  bLocal="));
    sysnum(pMonitorInit->bLocal);
    syslog(TEXT("\r\n"));
#endif
    /* return pMonitorInit as hMonitor */
    *phMonitor = (PHANDLE)pMonitorInit;
    return &mon2;
}

MONITORUI mui = {
    sizeof(MONITORUI),
    rcAddPortUI,
    rcConfigurePortUI,
    rcDeletePortUI
};

PORTMONEXPORT PMONITORUI WINAPI _export InitializePrintMonitorUI(VOID) {
#ifdef DEBUG_REDMON
    syslog(TEXT("InitializePrintMonitorUI\r\n"));
#endif
    return &mui;
}

/* DLL entry point for Borland C++ */
BOOL WINAPI _export DllEntryPoint(HINSTANCE hInst, DWORD fdwReason, LPVOID lpReserved) {
    hdll = hInst;
    DisableThreadLibraryCalls(hInst);
    return TRUE;
}

/* DLL entry point for Microsoft Visual C++ */
PORTMONEXPORT BOOL WINAPI DllMain(HINSTANCE hInst, DWORD fdwReason, LPVOID lpReserved) {
    return DllEntryPoint(hInst, fdwReason, lpReserved);
}
