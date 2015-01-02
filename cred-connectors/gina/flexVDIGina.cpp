/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <fstream>
#include <algorithm>
#include <windows.h>
extern "C" {
#include <winwlx.h>
}
#include "GinaDialogHooks.hpp"
#include "util.hpp"
using namespace flexvm;

#define LDB(text) Log(L_DEBUG) << text

// Location of the real MSGINA.
#define REALGINA_PATH      TEXT("MSGINA.DLL")
#define GINASTUB_VERSION   (DWORD)(WLX_VERSION_1_4)

// Function prototypes for the GINA interface.
typedef BOOL (WINAPI * PFWLXNEGOTIATE)(DWORD, DWORD *);
typedef BOOL (WINAPI * PFWLXINITIALIZE)(LPWSTR, HANDLE, PVOID, PVOID, PVOID *);
typedef VOID (WINAPI * PFWLXDISPLAYSASNOTICE)(PVOID);
typedef int  (WINAPI * PFWLXLOGGEDOUTSAS)(PVOID, DWORD, PLUID, PSID, PDWORD,
                                          PHANDLE, PWLX_MPR_NOTIFY_INFO, PVOID *);
typedef BOOL (WINAPI * PFWLXACTIVATEUSERSHELL)(PVOID, PWSTR, PWSTR, PVOID);
typedef int  (WINAPI * PFWLXLOGGEDONSAS)(PVOID, DWORD, PVOID);
typedef VOID (WINAPI * PFWLXDISPLAYLOCKEDNOTICE)(PVOID);
typedef int  (WINAPI * PFWLXWKSTALOCKEDSAS)(PVOID, DWORD);
typedef BOOL (WINAPI * PFWLXISLOCKOK)(PVOID);
typedef BOOL (WINAPI * PFWLXISLOGOFFOK)(PVOID);
typedef VOID (WINAPI * PFWLXLOGOFF)(PVOID);
typedef VOID (WINAPI * PFWLXSHUTDOWN)(PVOID, DWORD);

// New for version 1.1
typedef BOOL (WINAPI * PFWLXSCREENSAVERNOTIFY)(PVOID, BOOL *);
typedef BOOL (WINAPI * PFWLXSTARTAPPLICATION)(PVOID, PWSTR, PVOID, PWSTR);

// New for version 1.3
typedef BOOL (WINAPI * PFWLXNETWORKPROVIDERLOAD)(PVOID, PWLX_MPR_NOTIFY_INFO);
typedef BOOL (WINAPI * PFWLXDISPLAYSTATUSMESSAGE)(PVOID, HDESK, DWORD, PWSTR, PWSTR);
typedef BOOL (WINAPI * PFWLXGETSTATUSMESSAGE)(PVOID, DWORD *, PWSTR, DWORD);
typedef BOOL (WINAPI * PFWLXREMOVESTATUSMESSAGE)(PVOID);

// New for version 1.4
typedef BOOL (WINAPI * PFWLXGETCONSOLESWITCHCREDENTIALS)(PVOID, PVOID);
typedef VOID (WINAPI * PFWLXRECONNECTNOTIFY)(PVOID);
typedef VOID (WINAPI * PFWLXDISCONNECTNOTIFY)(PVOID);

// Winlogon function dispatch table.
static DWORD dwWlxVersion;

// Pointers to the real MSGINA functions.
static PFWLXNEGOTIATE                pfWlxNegotiate;
static PFWLXINITIALIZE               pfWlxInitialize;
static PFWLXDISPLAYSASNOTICE         pfWlxDisplaySASNotice;
static PFWLXLOGGEDOUTSAS             pfWlxLoggedOutSAS;
static PFWLXACTIVATEUSERSHELL        pfWlxActivateUserShell;
static PFWLXLOGGEDONSAS              pfWlxLoggedOnSAS;
static PFWLXDISPLAYLOCKEDNOTICE      pfWlxDisplayLockedNotice;
static PFWLXWKSTALOCKEDSAS           pfWlxWkstaLockedSAS;
static PFWLXISLOCKOK                 pfWlxIsLockOk;
static PFWLXISLOGOFFOK               pfWlxIsLogoffOk;
static PFWLXLOGOFF                   pfWlxLogoff;
static PFWLXSHUTDOWN                 pfWlxShutdown;

// New for version 1.1
static PFWLXSTARTAPPLICATION         pfWlxStartApplication  = NULL;
static PFWLXSCREENSAVERNOTIFY        pfWlxScreenSaverNotify = NULL;

// New for version 1.2 - No new GINA interface was added, except
//                       a new function in the dispatch table.

// New for version 1.3
static PFWLXNETWORKPROVIDERLOAD      pfWlxNetworkProviderLoad  = NULL;
static PFWLXDISPLAYSTATUSMESSAGE     pfWlxDisplayStatusMessage = NULL;
static PFWLXGETSTATUSMESSAGE         pfWlxGetStatusMessage     = NULL;
static PFWLXREMOVESTATUSMESSAGE      pfWlxRemoveStatusMessage  = NULL;

// New for 1.4
static PFWLXGETCONSOLESWITCHCREDENTIALS    pfWlxGetConsoleSwitchCredentials = NULL;
static PFWLXRECONNECTNOTIFY                pfWlxReconnectNotify = NULL;
static PFWLXDISCONNECTNOTIFY               pfWlxDisconnectNotify = NULL;


template <typename T>
static bool getFunctionPointer(T & p, HINSTANCE hDll, LPCSTR name) {
    p = (T)GetProcAddress(hDll, name);
    return p != NULL;
}
#define GET_POINTER_OR_RETURN(hDll, name) \
do { if (!getFunctionPointer(pf ## name, hDll, #name)) return FALSE; } while (0)

// Hook into the real MSGINA.
BOOL getGINAFunctionPointers(HINSTANCE hDll, DWORD dwWlxVersion) {
    // Get pointers to all of the WLX functions in the real MSGINA.
    GET_POINTER_OR_RETURN(hDll, WlxInitialize);
    GET_POINTER_OR_RETURN(hDll, WlxDisplaySASNotice);
    GET_POINTER_OR_RETURN(hDll, WlxLoggedOutSAS);
    GET_POINTER_OR_RETURN(hDll, WlxActivateUserShell);
    GET_POINTER_OR_RETURN(hDll, WlxLoggedOnSAS);
    GET_POINTER_OR_RETURN(hDll, WlxDisplayLockedNotice);
    GET_POINTER_OR_RETURN(hDll, WlxIsLockOk);
    GET_POINTER_OR_RETURN(hDll, WlxWkstaLockedSAS);
    GET_POINTER_OR_RETURN(hDll, WlxIsLogoffOk);
    GET_POINTER_OR_RETURN(hDll, WlxLogoff);
    GET_POINTER_OR_RETURN(hDll, WlxShutdown);

    // Load functions for version 1.1 as necessary.
    if (dwWlxVersion > WLX_VERSION_1_0) {
        GET_POINTER_OR_RETURN(hDll, WlxStartApplication);
        GET_POINTER_OR_RETURN(hDll, WlxScreenSaverNotify);
    }

    // Load functions for version 1.3 as necessary.
    if (dwWlxVersion > WLX_VERSION_1_2) {
        GET_POINTER_OR_RETURN(hDll, WlxNetworkProviderLoad);
        GET_POINTER_OR_RETURN(hDll, WlxDisplayStatusMessage);
        GET_POINTER_OR_RETURN(hDll, WlxGetStatusMessage);
        GET_POINTER_OR_RETURN(hDll, WlxRemoveStatusMessage);
    }

    // Load functions for version 1.4 as necessary.
    if (dwWlxVersion > WLX_VERSION_1_3) {
        GET_POINTER_OR_RETURN(hDll, WlxGetConsoleSwitchCredentials);
        GET_POINTER_OR_RETURN(hDll, WlxReconnectNotify);
        GET_POINTER_OR_RETURN(hDll, WlxDisconnectNotify);
    }

    return TRUE;
}

static std::ofstream logFile;
static const char * getLogPath() {
    static char logPath[1024];
    char tempPath[1024];
    GetTempPathA(1024, tempPath);
    sprintf_s(logPath, 1024, "%s\\flexvdi_gina.log", tempPath);
    return logPath;
}


BOOL WINAPI WlxNegotiate(DWORD dwWinlogonVersion, DWORD * pdwDllVersion) {
    logFile.open(getLogPath(), std::ios_base::app);
    logFile << std::endl << std::endl;
    Log::setLogOstream(&logFile);
    LogCall lc(__FUNCTION__);

    // Load MSGINA.DLL. TODO: Check if there is another custom GINA DLL.
    HINSTANCE hDll = LoadLibrary(REALGINA_PATH);
    return_if(!hDll, "LoadLibrary failed", FALSE);
    GET_POINTER_OR_RETURN(hDll, WlxNegotiate);

    // Handle older version of Winlogon.
    dwWlxVersion = std::min(GINASTUB_VERSION, dwWinlogonVersion);
    // Negotiate version with MSGINA, load the rest of the WLX functions.
    Log(L_DEBUG) << "Negotiate with original gina";
    if (!pfWlxNegotiate(dwWlxVersion, &dwWlxVersion) ||
        !getGINAFunctionPointers(hDll, dwWlxVersion)) {
        return FALSE;
    }
    *pdwDllVersion = dwWlxVersion;

    return TRUE;
}


BOOL WINAPI WlxInitialize(LPWSTR lpWinsta, HANDLE hWlx, PVOID pvReserved,
                          PVOID pWinlogonFunctions, PVOID * pWlxContext) {
    LogCall lc(__FUNCTION__);
    GinaDialogHooks::singleton().hookWinlogonFunctions(pWinlogonFunctions, dwWlxVersion, hWlx);
    return pfWlxInitialize(lpWinsta, hWlx, pvReserved,
                           pWinlogonFunctions, pWlxContext);
}


VOID WINAPI WlxDisplaySASNotice(PVOID pWlxContext) {
    LogCall lc(__FUNCTION__);
    pfWlxDisplaySASNotice(pWlxContext);
}


// #define WINLOGON_REGISTRY_SUBKEY L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon\\"
// int setRegistryValue(const char * key, const std::string & value) {
//     Log(L_DEBUG) << "Set registry value " << key << " = " << value;
//
//     HKEY hkey;
//     LONG status = RegOpenKey(HKEY_LOCAL_MACHINE, WINLOGON_REGISTRY_SUBKEY, &hkey);
//     return_code_if(status, status, "Failed to open winlogon registry key.", false);
//
//     status = RegSetValueExA(hkey, key, 0, REG_SZ, (LPBYTE)value.c_str(), value.length() + 1);
//     RegCloseKey(hkey);
//     return_code_if(status, status, "Failed to write registry key.", false);
//     return true;
// }


/*
http://msdn.microsoft.com/en-us/library/windows/desktop/aa380571%28v=vs.85%29.aspx
*/
int WINAPI WlxLoggedOutSAS(PVOID pWlxContext, DWORD dwSasType, PLUID pAuthenticationId,
                           PSID pLogonSid, PDWORD pdwOptions, PHANDLE phToken,
                           PWLX_MPR_NOTIFY_INFO pNprNotifyInfo, PVOID * pProfile) {
    LogCall lc(__FUNCTION__);
    int iRet = pfWlxLoggedOutSAS(pWlxContext, dwSasType, pAuthenticationId,
                                 pLogonSid, pdwOptions, phToken, pNprNotifyInfo,
                                 pProfile);
    return iRet;
}


BOOL WINAPI WlxActivateUserShell(PVOID pWlxContext, PWSTR pszDesktopName,
                                 PWSTR pszMprLogonScript, PVOID pEnvironment) {
    LogCall lc(__FUNCTION__);
    return pfWlxActivateUserShell(pWlxContext, pszDesktopName,
                                  pszMprLogonScript, pEnvironment);
}


int WINAPI WlxLoggedOnSAS(PVOID pWlxContext, DWORD dwSasType, PVOID pReserved) {
    LogCall lc(__FUNCTION__);
    int ret = pfWlxLoggedOnSAS(pWlxContext, dwSasType, pReserved);
    return ret;
}


VOID WINAPI WlxDisplayLockedNotice(PVOID pWlxContext) {
    LogCall lc(__FUNCTION__);
    pfWlxDisplayLockedNotice(pWlxContext);
}


BOOL WINAPI WlxIsLockOk(PVOID pWlxContext) {
    LogCall lc(__FUNCTION__);
    return pfWlxIsLockOk(pWlxContext);
}


int WINAPI WlxWkstaLockedSAS(PVOID pWlxContext, DWORD dwSasType) {
    LogCall lc(__FUNCTION__);
    return pfWlxWkstaLockedSAS(pWlxContext, dwSasType);
}


BOOL WINAPI WlxIsLogoffOk(PVOID pWlxContext) {
    LogCall lc(__FUNCTION__);
    BOOL bSuccess = pfWlxIsLogoffOk(pWlxContext);
    if (bSuccess) {
        //
        // If it's OK to logoff, make sure stored credentials are cleaned up.
        //
    }
    return bSuccess;
}


VOID WINAPI WlxLogoff(PVOID pWlxContext) {
    LogCall lc(__FUNCTION__);
    pfWlxLogoff(pWlxContext);
}


VOID WINAPI WlxShutdown(PVOID pWlxContext, DWORD ShutdownType) {
    LogCall lc(__FUNCTION__);
    pfWlxShutdown(pWlxContext, ShutdownType);
}


//
// New for version 1.1
//

BOOL WINAPI WlxScreenSaverNotify(PVOID  pWlxContext, BOOL * pSecure) {
    LogCall lc(__FUNCTION__);
    return pfWlxScreenSaverNotify(pWlxContext, pSecure);
}

BOOL WINAPI WlxStartApplication(PVOID pWlxContext, PWSTR pszDesktopName,
                                PVOID pEnvironment, PWSTR pszCmdLine) {
    LogCall lc(__FUNCTION__);
    return pfWlxStartApplication(pWlxContext, pszDesktopName,
                                 pEnvironment, pszCmdLine);
}


//
// New for version 1.3
//

BOOL WINAPI WlxNetworkProviderLoad(PVOID pWlxContext,
                                   PWLX_MPR_NOTIFY_INFO pNprNotifyInfo) {
    LogCall lc(__FUNCTION__);
    return pfWlxNetworkProviderLoad(pWlxContext, pNprNotifyInfo);
}


BOOL WINAPI WlxDisplayStatusMessage(PVOID pWlxContext, HDESK hDesktop, DWORD dwOptions,
                                    PWSTR pTitle, PWSTR pMessage) {
    LogCall lc(__FUNCTION__);
    return pfWlxDisplayStatusMessage(pWlxContext, hDesktop,  dwOptions,
                                     pTitle, pMessage);
}


BOOL WINAPI WlxGetStatusMessage(PVOID pWlxContext, DWORD * pdwOptions,
                                PWSTR pMessage, DWORD dwBufferSize) {
    LogCall lc(__FUNCTION__);
    return pfWlxGetStatusMessage(pWlxContext, pdwOptions,
                                 pMessage, dwBufferSize);
}

BOOL WINAPI WlxRemoveStatusMessage(PVOID pWlxContext) {
    LogCall lc(__FUNCTION__);
    return pfWlxRemoveStatusMessage(pWlxContext);
}

// New for version 1.4
//
BOOL WINAPI WlxGetConsoleSwitchCredentials(PVOID pWlxContext, PVOID p2) {
    LogCall lc(__FUNCTION__);
    return pfWlxGetConsoleSwitchCredentials(pWlxContext, p2);
}


VOID WINAPI WlxReconnectNotify(PVOID pWlxContext) {
    LogCall lc(__FUNCTION__);
    pfWlxReconnectNotify(pWlxContext);
}

VOID WINAPI WlxDisconnectNotify(PVOID pWlxContext) {
    LogCall lc(__FUNCTION__);
    pfWlxDisconnectNotify(pWlxContext);
}
