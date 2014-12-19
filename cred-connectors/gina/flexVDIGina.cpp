/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <fstream>
#include <algorithm>
#include <windows.h>
extern "C" {
#include <winwlx.h>
}
// #include "RegistryHelper.hpp"
#include "SSO.hpp"
#include "util.hpp"
using namespace flexvm;

#define LDB(text) Log(L_DEBUG) << text

// Location of the real MSGINA.
#define REALGINA_PATH      TEXT("MSGINA.DLL")
#define GINASTUB_VERSION   (DWORD)(WLX_VERSION_1_4)
#define WINLOGON_REGISTRY_SUBKEY L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon\\"

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
//     logFile.open("c:\\flexvdi_gina.log", std::ios_base::app);
    logFile << std::endl << std::endl;
    Log::setLogOstream(&logFile);

    LDB("-->WlxNegotiate");

    // Load MSGINA.DLL. TODO: Check if there is another custom GINA DLL.
    HINSTANCE hDll = LoadLibrary(REALGINA_PATH);
    return_if(!hDll, "LoadLibrary failed", FALSE);

//     pfWlxNegotiate = (PFWLXNEGOTIATE) GetProcAddress(hDll, "WlxNegotiate");
//     if (!pfWlxNegotiate) {
//         return FALSE;
//     }
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

    LDB("<--WlxNegotiate");
    return TRUE;
}


BOOL WINAPI WlxInitialize(LPWSTR lpWinsta, HANDLE hWlx, PVOID pvReserved,
                          PVOID pWinlogonFunctions, PVOID * pWlxContext) {
    LDB("-->WlxInitialize");

    // Save pointer to dispatch table.
    //
    // Note that g_pWinlogon will need to be properly casted to the
    // appropriate version when used to call function in the dispatch
    // table.
    //
    // For example, assuming we are at WLX_VERSION_1_3, we would call
    // WlxSasNotify() as follows:
    //
    // ((PWLX_DISPATCH_VERSION_1_3) g_pWinlogon)->WlxSasNotify(hWlx, MY_SAS);
    //
    SSO::singleton().hookWinlogonFunctions(pWinlogonFunctions, dwWlxVersion, hWlx);
    BOOL retVal = pfWlxInitialize(lpWinsta, hWlx, pvReserved,
                                  pWinlogonFunctions, pWlxContext);
    LDB("<--WlxInitialize");
    return retVal;

}


VOID WINAPI WlxDisplaySASNotice(PVOID pWlxContext) {
    LDB("-->WlxDisplaySASNotice");
//     // Aquí no puedo autenticar, y terminar el proceso de login porque no tengo forma de pasar el token
//     // resultado de la autenticación a winlogon
//     // El método adecuado para autenticar es el loggedOutSAS
//     // Pero puedo leer ya las credenciales, y si las tengo no invocar al WlxDisplaySASNotice
//     // mostrar ventanita que parece que hace que no esperemos
//
//     SSO::readCredentials();
//
//     // If there are no credentials, tell user to pulse "CAD"
//
//     // flex: First idea was to hide the "pulse CAD" alert.
//     // It was ok after system start, but when user logged off, system became hanged,
//     // shutdown button stopped working and CAD did nothing. You must do Force off.
//     // So we don't hide The alert anymore...
//
//     if (!SSO::justStarted || !SSO::usableCredentials) {
//         LDB("WlxDisplaySASNotice: No usable credentials. Asking for CAD");
//         /*NoticeDialog dlg(_pWinLogon, IDD_SASNOTICE);
//         dlg.Show();*/
//
//         //Ocultamos el pulse CAD al iniciar el guest si hay credenciales, poniendo el DisplaySAS dentro del if.
        pfWlxDisplaySASNotice(pWlxContext);
//     }
//
//     SSO::justStarted = false;
//
    LDB("<--WlxDisplaySASNotice");
}


/*
http://msdn.microsoft.com/en-us/library/windows/desktop/aa380571%28v=vs.85%29.aspx
*/
int WINAPI WlxLoggedOutSAS(PVOID pWlxContext, DWORD dwSasType, PLUID pAuthenticationId,
                           PSID pLogonSid, PDWORD pdwOptions, PHANDLE phToken,
                           PWLX_MPR_NOTIFY_INFO pNprNotifyInfo, PVOID * pProfile) {
    LDB("-->LoggedOutSAS(" << dwSasType << ")");

    /*
     * El SSO trata el caso de hacer un login automático cuando tenemos credenciales.
     * En caso contrario pasamos la invocación al MSGina para que abra diálogos, autentique...
     */
    // El SAS que se genera al iniciarse el XP es de tipo CTRL_ALT_DEL. Lo añado a la condicion.
    // Si fuese de tipo "introducida smart-card" no debería actuar nuestro Gina
//     if (WLX_SAS_TYPE_CTRL_ALT_DEL == dwSasType && SSO::usableCredentials) {
//
//         RegistryHelper::setRegistryValueInLocalMachine(WINLOGON_REGISTRY_SUBKEY, L"DefaultUserName", SSO::flexUser);
//         LDB1(L"Gina::guardo defaultUserName registry: -%s-", SSO::flexUser);
//         RegistryHelper::setRegistryValueInLocalMachine(WINLOGON_REGISTRY_SUBKEY, L"DefaultDomainName", SSO::flexDomain);
//         LDB1(L"Gina::guardo defaultUserName registry: -%s-", SSO::flexDomain);
//         RegistryHelper::setRegistryValueInLocalMachine(WINLOGON_REGISTRY_SUBKEY, L"DefaultPassword", SSO::flexPassword);
//
//         // AutoAdminLogon parece que no pero tambien es String
//         RegistryHelper::setRegistryValueInLocalMachine(WINLOGON_REGISTRY_SUBKEY, L"AutoAdminLogon", L"1");
//         LDB1(L"Gina::guardo AutoAdminLogon registry: -%s-", L"1");
//         //AutoAdminLogon
//     }

    int iRet;
    iRet = pfWlxLoggedOutSAS(pWlxContext, dwSasType, pAuthenticationId,
                             pLogonSid, pdwOptions, phToken, pNprNotifyInfo,
                             pProfile);

    if (iRet == WLX_SAS_ACTION_LOGON) {
        //
        // Copy pMprNotifyInfo and pLogonSid for later use.
        //

        // pMprNotifyInfo->pszUserName
        // pMprNotifyInfo->pszDomain
        // pMprNotifyInfo->pszPassword
        // pMprNotifyInfo->pszOldPassword
    }

//     //Limpiamos la password, y marcamos que no vuelva al automatico
//     RegistryHelper::setRegistryValueInLocalMachine(WINLOGON_REGISTRY_SUBKEY, L"DefaultPassword", L"");
//
//     // AutoAdminLogon parece que no pero tambien es String
//     RegistryHelper::setRegistryValueInLocalMachine(WINLOGON_REGISTRY_SUBKEY, L"AutoAdminLogon", L"0");
//     LDB1(L"Gina::guardo AutoAdminLogon registry: -%s-", L"1");
//     //AutoAdminLogon

    LDB("<--LoggedOutSAS");
    return iRet;
}


BOOL WINAPI WlxActivateUserShell(PVOID pWlxContext, PWSTR pszDesktopName,
                                 PWSTR pszMprLogonScript, PVOID pEnvironment) {
    LDB("-->WlxActivateUserShell");
    BOOL retVal = pfWlxActivateUserShell(pWlxContext, pszDesktopName,
                                         pszMprLogonScript, pEnvironment);
    LDB("<--WlxActivateUserShell");
    return retVal;
}


int WINAPI WlxLoggedOnSAS(PVOID pWlxContext, DWORD dwSasType, PVOID pReserved) {
    LDB("-->WlxLoggedOnSAS");
    int iRet = pfWlxLoggedOnSAS(pWlxContext, dwSasType, pReserved);
    LDB("<--WlxLoggedOnSAS");
    return iRet;
}


VOID WINAPI WlxDisplayLockedNotice(PVOID pWlxContext) {
    LDB("-->WlxDisplayLockedNotice");
    pfWlxDisplayLockedNotice(pWlxContext);
    LDB("<--WlxDisplayLockedNotice");
}


BOOL WINAPI WlxIsLockOk(PVOID pWlxContext) {
    LDB("-->WlxIsLockOk");
    BOOL bRet = pfWlxIsLockOk(pWlxContext);
    LDB("<--WlxIsLockOk");
    return  bRet;
}


int WINAPI WlxWkstaLockedSAS(PVOID pWlxContext, DWORD dwSasType) {
    LDB("-->WlxWkstaLockedSAS");
    int iRet = pfWlxWkstaLockedSAS(pWlxContext, dwSasType);
    LDB("<--WlxWkstaLockedSAS");
    return  iRet;
}


BOOL WINAPI WlxIsLogoffOk(PVOID pWlxContext) {
    LDB("-->WlxIsLogoffOk");
    BOOL bSuccess = pfWlxIsLogoffOk(pWlxContext);
    if (bSuccess) {
        //
        // If it's OK to logoff, make sure stored credentials are cleaned up.
        //
    }
    LDB("<--WlxIsLogoffOk");
    return bSuccess;
}


VOID WINAPI WlxLogoff(PVOID pWlxContext) {
    LDB("<--WlxLogoff");
    pfWlxLogoff(pWlxContext);
    LDB("-->WlxLogoff");
}


VOID WINAPI WlxShutdown(PVOID pWlxContext, DWORD ShutdownType) {
    // Limpieza de variables de flexVDIGina
    // Currently  nothing to do

    LDB("-->pfWlxShutdown");
    pfWlxShutdown(pWlxContext, ShutdownType);
    LDB("<--pfWlxShutdown");
}


//
// New for version 1.1
//

BOOL WINAPI WlxScreenSaverNotify(PVOID  pWlxContext, BOOL * pSecure) {
    LDB("-->WlxScreenSaverNotify");
    BOOL bRet = pfWlxScreenSaverNotify(pWlxContext, pSecure);
    LDB("<--WlxScreenSaverNotify");
    return bRet;
}

BOOL WINAPI WlxStartApplication(PVOID pWlxContext, PWSTR pszDesktopName,
                                PVOID pEnvironment, PWSTR pszCmdLine) {
    LDB("<--WlxStartApplication");
    BOOL bRet = pfWlxStartApplication(pWlxContext, pszDesktopName,
                                      pEnvironment, pszCmdLine);
    LDB("<--WlxStartApplication");
    return bRet;
}


//
// New for version 1.3
//

BOOL WINAPI WlxNetworkProviderLoad(PVOID pWlxContext,
                                   PWLX_MPR_NOTIFY_INFO pNprNotifyInfo) {
    LDB("<--WlxNetworkProviderLoad");
    BOOL bRet = pfWlxNetworkProviderLoad(pWlxContext, pNprNotifyInfo);
    LDB("<--WlxNetworkProviderLoad");
    return bRet;
}


BOOL WINAPI WlxDisplayStatusMessage(PVOID pWlxContext, HDESK hDesktop, DWORD dwOptions,
                                    PWSTR pTitle, PWSTR pMessage) {
    LDB("<--WlxDisplayStatusMessage");
    BOOL bRet = pfWlxDisplayStatusMessage(pWlxContext, hDesktop,  dwOptions,
                                          pTitle, pMessage);
    LDB("<--WlxDisplayStatusMessage");
    return bRet;
}


BOOL WINAPI WlxGetStatusMessage(PVOID pWlxContext, DWORD * pdwOptions,
                                PWSTR pMessage, DWORD dwBufferSize) {
    LDB("<--WlxGetStatusMessage");
    BOOL bRet = pfWlxGetStatusMessage(pWlxContext, pdwOptions,
                                      pMessage, dwBufferSize);
    LDB("<--WlxGetStatusMessage");

    return bRet;
}

BOOL WINAPI WlxRemoveStatusMessage(PVOID pWlxContext) {
    LDB("<--WlxRemoveStatusMessage");
    BOOL bRet =  pfWlxRemoveStatusMessage(pWlxContext);
    LDB("<--WlxRemoveStatusMessage");
    return bRet;
}

// New for version 1.4
//
BOOL WINAPI WlxGetConsoleSwitchCredentials(PVOID pWlxContext, PVOID p2) {
    LDB("<--WlxGetConsoleSwitchCredentials");
    BOOL bRet = pfWlxGetConsoleSwitchCredentials(pWlxContext, p2);
    LDB("<--WlxGetConsoleSwitchCredentials");
    return bRet;
}


VOID WINAPI WlxReconnectNotify(PVOID pWlxContext) {
    LDB("<--WlxReconnectNotify");
    pfWlxReconnectNotify(pWlxContext);
    LDB("<--WlxReconnectNotify");
}

VOID WINAPI WlxDisconnectNotify(PVOID pWlxContext) {
    LDB("<--WlxDisconnectNotify");
    SSO::singleton().stopListening();
    pfWlxDisconnectNotify(pWlxContext);
    LDB("<--WlxDisconnectNotify");
}
