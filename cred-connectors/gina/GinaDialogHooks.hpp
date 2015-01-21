/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _GINADIALOGHOOKS_HPP_
#define _GINADIALOGHOOKS_HPP_

#include <memory>
#include <windows.h>
#include "../CredentialsThread.hpp"

namespace flexvm {

class GinaDialogHooks : public CredentialsThread::Listener {
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

    virtual void credentialsChanged(const char * u, const char * p, const char * d);

    std::string username, password, domain;
    CRITICAL_SECTION mutex;
    CredentialsThread thread;
    std::unique_ptr<BaseWinlogonProxy> winlogon;
    DWORD wlxVersion;
    HANDLE hWlx;
    DLGPROC currentDlgProc;
    HWND currentDialog;
    int usernameIDC, passwordIDC, domainIDC;

    GinaDialogHooks() : thread(*this) {
        InitializeCriticalSection(&mutex);
    }
    void sendCtrlAltDel() {
        winlogon->sendCtrlAltDel(hWlx);
    }
    void findCredentialControls(HWND hwndDlg);
    static int WINAPI WlxDialogBoxParam(HANDLE hWlx, HANDLE hInst, LPWSTR lpszTemplate,
                                        HWND hwndOwner, DLGPROC dlgprc, LPARAM  dwInitParam);
    static INT_PTR CALLBACK PassDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    BOOL LogonDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void clearCredentials() {
        for (auto & c : username) c = 0;
        for (auto & c : password) c = 0;
        for (auto & c : domain) c = 0;
    }
};

} // namespace flexvm

#endif // _GINADIALOGHOOKS_HPP_
