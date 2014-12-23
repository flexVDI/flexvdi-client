/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <windows.h>
#include <winwlx.h>
#include "FlexVDIProto.h"
#include "SSO.hpp"
#include "util.hpp"

using namespace flexvm;

#define CRED_FILE_PATH L"\\\\.\\pipe\\flexvdi_creds"

struct Lock {
    CRITICAL_SECTION & mutex;
    Lock(CRITICAL_SECTION & m) : mutex(m) {
        EnterCriticalSection(&mutex);
    }
    ~Lock() {
        LeaveCriticalSection(&mutex);
    }
};


template <typename T>
class WinlogonProxy : public SSO::BaseWinlogonProxy {
public:
    WinlogonProxy(PVOID winlogonFunctions) {
        functions = (T)winlogonFunctions;
        origWlxDialogBoxParam = functions->WlxDialogBoxParam;
        functions->WlxDialogBoxParam = getWlxDialogBoxParamFunc();
    }

    virtual int WINAPI WlxDialogBoxParam(HANDLE hWlx, HANDLE hInst,
                                         LPWSTR lpszTemplate, HWND hwndOwner,
                                         DLGPROC dlgprc, LPARAM dwInitParam) {
        return origWlxDialogBoxParam(hWlx, hInst, lpszTemplate,
                                     hwndOwner, dlgprc, dwInitParam);
    }

    virtual void sendCtrlAltDel(HANDLE hWlx) {
        functions->WlxSasNotify(hWlx, WLX_SAS_TYPE_CTRL_ALT_DEL);
    }

private:
    T functions;
    PWLX_DIALOG_BOX_PARAM origWlxDialogBoxParam;
};


SSO::SSO() : attemptedLogin(false) {
    Log(L_DEBUG) << "SSO::ctor ";
    InitializeCriticalSection(&mutex);
}


void SSO::hookWinlogonFunctions(PVOID pWinlogonFunctions, DWORD dwWlxVersion, HANDLE a_hWlx) {
    hWlx = a_hWlx;
    wlxVersion = dwWlxVersion;
    switch (dwWlxVersion) {
        case WLX_VERSION_1_0:
            winlogon.reset(new WinlogonProxy<PWLX_DISPATCH_VERSION_1_0>(pWinlogonFunctions));
            break;
        case WLX_VERSION_1_1:
            winlogon.reset(new WinlogonProxy<PWLX_DISPATCH_VERSION_1_1>(pWinlogonFunctions));
            break;
        case WLX_VERSION_1_2:
            winlogon.reset(new WinlogonProxy<PWLX_DISPATCH_VERSION_1_2>(pWinlogonFunctions));
            break;
        case WLX_VERSION_1_3:
            winlogon.reset(new WinlogonProxy<PWLX_DISPATCH_VERSION_1_3>(pWinlogonFunctions));
            break;
        case WLX_VERSION_1_4:
        default:
            winlogon.reset(new WinlogonProxy<PWLX_DISPATCH_VERSION_1_4>(pWinlogonFunctions));
            break;
    }
}


void SSO::stopListening() {
    if (thread != NULL) {
        TerminateThread(thread, 0);
        thread = NULL;
        threadId = 0;
        CloseHandle(pipe);
        pipe = NULL;
    }
}


DWORD SSO::readCredentials(HWND window) {
    static const int MAX_CRED_SIZE = 1024;

    Log(L_DEBUG) << "About to read credentials";
    // TODO: configurable
    static const char * pipeName = "\\\\.\\pipe\\flexvdi_creds";
    pipe = CreateFileA(pipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    return_if(pipe == INVALID_HANDLE_VALUE, "Error opening pipe", -1);

    uint8_t buffer[MAX_CRED_SIZE];
    DWORD bytesRead;
    BOOL success = ReadFile(pipe, (char *)buffer, MAX_CRED_SIZE, &bytesRead, NULL);
    CloseHandle(pipe);
    pipe = NULL;
    return_if(!success, "Reading credentials", -1);

    FlexVDICredentialsMsg * msg = (FlexVDICredentialsMsg *)buffer;
    return_if(!marshallMessage(FLEXVDI_CREDENTIALS, buffer, bytesRead),
              "Message size mismatch", -1);
    Log(L_DEBUG) << "Credentials received";

    {
        Lock l(mutex);
        username = getCredentialsUser(msg);
        password = getCredentialsPassword(msg);
        domain = getCredentialsDomain(msg);
    }

    PostMessage(window, WM_USER + 5, 0, 0);
    return 0;
}


int WINAPI SSO::WlxDialogBoxParam(HANDLE hWlx, HANDLE hInst, LPWSTR lpszTemplate,
                                  HWND hwndOwner, DLGPROC dlgprc, LPARAM dwInitParam) {
    SSO & s = SSO::singleton();
    s.currentDlgProc = dlgprc;
    int res = s.winlogon->WlxDialogBoxParam(hWlx, hInst, lpszTemplate,
                                            hwndOwner, PassDlgProc, dwInitParam);
    s.stopListening();
    Log(L_DEBUG) << "<--WlxDialogBoxParam (" << LOWORD(lpszTemplate) << ")";
    return res;
}


BOOL CALLBACK SSO::PassDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    SSO & s = SSO::singleton();
    BOOL result = s.LogonDlgProc(hwndDlg, uMsg, wParam, lParam);
    if (uMsg == WM_INITDIALOG && s.passwordIDC == 0) {
        // This may be the annoying dialog askig for Ctrl+Alt+Del
        // This is a bit useless, because it can just be disabled.
        s.sendCtrlAltDel();
    }
    return result;
}


BOOL SSO::LogonDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_INITDIALOG) {
        findCredentialControls(hwndDlg);
        thread = CreateThread(NULL, 0, &listeningThread, hwndDlg, 0, &threadId);
    }

    if (uMsg == WM_USER + 5) {
        Lock l(mutex);
        SetDlgItemTextA(hwndDlg, usernameIDC, username.c_str());
        SetDlgItemTextA(hwndDlg, passwordIDC, password.c_str());
        SetDlgItemTextA(hwndDlg, domainIDC, domain.c_str());
        password = "";
        username = "";
        domain = "";
        uMsg = WM_COMMAND;
        wParam = IDOK;
        lParam = 0;
    }

    return currentDlgProc(hwndDlg, uMsg, wParam, lParam);
}


void SSO::findCredentialControls(HWND hwndDlg) {
    // Assume there are two edit controls, one of them with password style,
    // and a combobox for the domain
    HWND hwnd = NULL;
    passwordIDC = usernameIDC = domainIDC = 0;
    for (int i = 0; i < 2; ++i) {
        WINDOWINFO info;
        hwnd = FindWindowExA(hwndDlg, hwnd, "EDIT", NULL);
        if (hwnd && GetWindowInfo(hwnd, &info) && (info.dwStyle & ES_PASSWORD)) {
            passwordIDC = GetDlgCtrlID(hwnd);
            Log(L_DEBUG) << "This is the password control: " << passwordIDC;
        } else if (hwnd) {
            usernameIDC = GetDlgCtrlID(hwnd);
        }
    }
    hwnd = FindWindowExA(hwndDlg, NULL, "COMBOBOX", NULL);
    if (hwnd) {
        domainIDC = GetDlgCtrlID(hwnd);
    }
}
