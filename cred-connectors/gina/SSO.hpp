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

    static const int MAX_CRED_SIZE = 1024;

    bool attemptedLogin;
    HANDLE thread;
    DWORD threadId;
    std::unique_ptr<BaseWinlogonProxy> winlogon;
    DWORD wlxVersion;
    HANDLE hWlx;
    HANDLE pipe;
    DLGPROC pfWlxLoggedOutSASDlgProc;
    DLGPROC pfWlxDisplaySASNoticeDlgProc;
    DLGPROC pfWlxDisplayLockedNoticeDlgProc;
    DLGPROC pfWlxLoggedOnSASDlgProc;
    DLGPROC pfWlxUserMessageDlgProc;

    SSO();
    DWORD readCredentials(HWND window);
    void sendCtrlAltDel() {
        winlogon->sendCtrlAltDel(hWlx);
    }

    static int WINAPI WlxDialogBoxParam(HANDLE hWlx, HANDLE hInst, LPWSTR lpszTemplate,
                                        HWND hwndOwner, DLGPROC dlgprc, LPARAM  dwInitParam);
    static BOOL CALLBACK WlxLoggedOutSASDlgProc(HWND hwndDlg, UINT uMsg,
                                                WPARAM wParam, LPARAM lParam);
    static BOOL CALLBACK WlxDisplaySASNoticeDlgProc(HWND hwndDlg, UINT uMsg,
                                                    WPARAM wParam, LPARAM lParam);
    static BOOL CALLBACK WlxDisplayLockedNoticeDlgProc(HWND hwndDlg, UINT uMsg,
                                                       WPARAM wParam, LPARAM lParam);
    static BOOL CALLBACK WlxLoggedOnSASDlgProc(HWND hwndDlg, UINT uMsg,
                                               WPARAM wParam, LPARAM lParam);
    static BOOL CALLBACK WlxUserMessageDlgProc(HWND hwndDlg, UINT uMsg,
                                               WPARAM wParam, LPARAM lParam);
    BOOL LogonDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam,
                      DLGPROC dlgProc, int usernameIDC, int passwordIDC);

    static DWORD WINAPI listeningThread(LPVOID lpParameter) {
        return SSO::singleton().readCredentials((HWND)lpParameter);
    }
};

} // namespace flexvm

#endif // _SSO_HPP_
