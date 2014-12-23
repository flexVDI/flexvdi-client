/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _SSO_HPP_
#define _SSO_HPP_

#include <memory>
#include <wchar.h>

namespace flexvm {

class SSO {
public:
    static SSO & singleton() {
        static SSO instance;
        return instance;
    }
    void hookWinlogonFunctions(PVOID pWinlogonFunctions, DWORD dwWlxVersion, HANDLE a_hWlx);
    void stopListening();

private:
    typedef int WINAPI (*WlxDialogBoxParamFuncPtr)(HANDLE, HANDLE, LPWSTR,
                                                   HWND, DLGPROC, LPARAM);
    class BaseWinlogonProxy {
    public:
        virtual int WINAPI WlxDialogBoxParam(HANDLE hWlx, HANDLE hInst,
                                             LPWSTR lpszTemplate, HWND hwndOwner,
                                             DLGPROC dlgprc, LPARAM dwInitParam) = 0;
        virtual void sendCtrlAltDel(HANDLE hWlx) = 0;

    protected:
        BaseWinlogonProxy() {}
        WlxDialogBoxParamFuncPtr getWlxDialogBoxParamFunc() const {
            return SSO::WlxDialogBoxParam;
        }
    };
    friend BaseWinlogonProxy;

    std::string username, password, domain;
    CRITICAL_SECTION mutex;
    bool attemptedLogin;
    HANDLE thread;
    DWORD threadId;
    std::unique_ptr<BaseWinlogonProxy> winlogon;
    DWORD wlxVersion;
    HANDLE hWlx;
    HANDLE pipe;
    DLGPROC currentDlgProc;
    int usernameIDC, passwordIDC, domainIDC;

    SSO();
    DWORD readCredentials(HWND window);
    void sendCtrlAltDel() {
        winlogon->sendCtrlAltDel(hWlx);
    }
    void findCredentialControls(HWND hwndDlg);

    static int WINAPI WlxDialogBoxParam(HANDLE hWlx, HANDLE hInst, LPWSTR lpszTemplate,
                                        HWND hwndOwner, DLGPROC dlgprc, LPARAM  dwInitParam);
    static BOOL CALLBACK PassDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    BOOL LogonDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static DWORD WINAPI listeningThread(LPVOID lpParameter) {
        return SSO::singleton().readCredentials((HWND)lpParameter);
    }
};

} // namespace flexvm

#endif // _SSO_HPP_
