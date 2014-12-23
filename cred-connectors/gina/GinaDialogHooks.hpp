/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _GINADIALOGHOOKS_HPP_
#define _GINADIALOGHOOKS_HPP_

#include <memory>
#include <windows.h>
#include "CredentialsConnectorThread.hpp"

namespace flexvm {

class GinaDialogHooks {
public:
    static GinaDialogHooks & singleton() {
        static GinaDialogHooks instance;
        return instance;
    }
    void hookWinlogonFunctions(PVOID pWinlogonFunctions, DWORD dwWlxVersion, HANDLE a_hWlx);

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
            return GinaDialogHooks::WlxDialogBoxParam;
        }
    };
    friend BaseWinlogonProxy;

    Credentials creds;
    CredentialsConnectorThread thread;
    std::unique_ptr<BaseWinlogonProxy> winlogon;
    DWORD wlxVersion;
    HANDLE hWlx;
    DLGPROC currentDlgProc;
    int usernameIDC, passwordIDC, domainIDC;

    GinaDialogHooks() : thread(creds) {}
    void sendCtrlAltDel() {
        winlogon->sendCtrlAltDel(hWlx);
    }
    void findCredentialControls(HWND hwndDlg);
    static int WINAPI WlxDialogBoxParam(HANDLE hWlx, HANDLE hInst, LPWSTR lpszTemplate,
                                        HWND hwndOwner, DLGPROC dlgprc, LPARAM  dwInitParam);
    static BOOL CALLBACK PassDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    BOOL LogonDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

} // namespace flexvm

#endif // _GINADIALOGHOOKS_HPP_
