/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _CREDENTIALSCONNECTORTHREAD_HPP_
#define _CREDENTIALSCONNECTORTHREAD_HPP_

#include <string>
#include <windows.h>

namespace flexvm {

class Credentials {
public:
    Credentials() {
        InitializeCriticalSection(&mutex);
    }

    void setCredentials(const std::string & u, const std::string & p, const std::string & d) {
        EnterCriticalSection(&mutex);
        username = u;
        password = p;
        domain = d;
        PostMessage(currentDialog, WM_USER + 5, 0, 0);
        LeaveCriticalSection(&mutex);
    }

    void getCredentials(std::string & u, std::string & p, std::string & d) {
        EnterCriticalSection(&mutex);
        u = username;
        p = password;
        d = domain;
        password = username = domain = "";
        LeaveCriticalSection(&mutex);
    }

    void setCurrentDialog(HWND cDlg) {
        EnterCriticalSection(&mutex);
        currentDialog = cDlg;
        LeaveCriticalSection(&mutex);
    }
protected:
    std::string username, password, domain;
    CRITICAL_SECTION mutex;
    HWND currentDialog;
};


class CredentialsConnectorThread {
public:
    CredentialsConnectorThread(Credentials & creds) : creds(creds) {}

    void start();
    void stop();

private:
    Credentials & creds;
    HANDLE thread;
    DWORD threadId;
    HANDLE pipe;

    DWORD readCredentials();
    static DWORD WINAPI threadProc(LPVOID lpParameter) {
        return ((CredentialsConnectorThread *)lpParameter)->readCredentials();
    }
};

} // namespace flexvm

#endif // _CREDENTIALSCONNECTORTHREAD_HPP_
