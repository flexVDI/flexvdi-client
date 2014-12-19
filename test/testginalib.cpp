#include <iostream>
#include <system_error>
#include "windows.h"
#include "winwlx.h"
using namespace std;

BOOL lookupFunctions(HMODULE hDll) {
    if (!GetProcAddress(hDll, "WlxNegotiate")) return FALSE;
    if (!GetProcAddress(hDll, "WlxInitialize")) return FALSE;
    if (!GetProcAddress(hDll, "WlxDisplaySASNotice")) return FALSE;
    if (!GetProcAddress(hDll, "WlxLoggedOutSAS")) return FALSE;
    if (!GetProcAddress(hDll, "WlxActivateUserShell")) return FALSE;
    if (!GetProcAddress(hDll, "WlxLoggedOnSAS")) return FALSE;
    if (!GetProcAddress(hDll, "WlxDisplayLockedNotice")) return FALSE;
    if (!GetProcAddress(hDll, "WlxIsLockOk")) return FALSE;
    if (!GetProcAddress(hDll, "WlxWkstaLockedSAS")) return FALSE;
    if (!GetProcAddress(hDll, "WlxIsLogoffOk")) return FALSE;
    if (!GetProcAddress(hDll, "WlxLogoff")) return FALSE;
    if (!GetProcAddress(hDll, "WlxShutdown")) return FALSE;
    if (!GetProcAddress(hDll, "WlxStartApplication")) return FALSE;
    if (!GetProcAddress(hDll, "WlxScreenSaverNotify")) return FALSE;
    if (!GetProcAddress(hDll, "WlxNetworkProviderLoad")) return FALSE;
    if (!GetProcAddress(hDll, "WlxDisplayStatusMessage")) return FALSE;
    if (!GetProcAddress(hDll, "WlxGetStatusMessage")) return FALSE;
    if (!GetProcAddress(hDll, "WlxRemoveStatusMessage")) return FALSE;
    if (!GetProcAddress(hDll, "WlxGetConsoleSwitchCredentials")) return FALSE;
    if (!GetProcAddress(hDll, "WlxReconnectNotify")) return FALSE;
    if (!GetProcAddress(hDll, "WlxDisconnectNotify")) return FALSE;

    // Everything loaded OK.
    return TRUE;
}

typedef BOOL (WINAPI * PFWLXNEGOTIATE)(DWORD, DWORD *);

int main(int argc, char * argv[]) {
    if (argc != 2) {
        cout << "Dll missing" << endl;
        return 1;
    }
    HMODULE hDll = LoadLibraryA(argv[1]);
    if (hDll == NULL) {
        cout << "LoadLibrary: " << std::system_category().message(GetLastError());
        return 1;
    }
    if (!lookupFunctions(hDll)) {
        cout << "GetProcAddress: " << std::system_category().message(GetLastError());
        return 1;
    }
    cout << "Found" << endl;
    PFWLXNEGOTIATE pfWlxNegotiate = (PFWLXNEGOTIATE)GetProcAddress(hDll, "WlxNegotiate");
    DWORD dwWlxVersion = WLX_VERSION_1_4;
    if (!pfWlxNegotiate(dwWlxVersion, &dwWlxVersion)) {
        cout << "WlxNegotiate: " << std::system_category().message(GetLastError());
        return 1;
    }
    cout << "Wlx version = " << dwWlxVersion << endl;
    return 0;
}
