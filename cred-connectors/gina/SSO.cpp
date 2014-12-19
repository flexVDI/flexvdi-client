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
// // Data for sysprep. Lo borramos en la dll. Leído por windows durante la creación de la máquina.
// #define SYSPREP_FILE_PATH L"A:\\sysprep.inf"

// From oVirt GinaSSO:
// MSGINA dialog box IDs.
#define DEFAULT_IDD_WLXDISPLAYSASNOTICE_DIALOG      1400
// updated by Itai Shaham 02.09.07
#define DEFAULT_IDD_WLXLOGGEDOUTSAS_DIALOG          1500
// added by Rudy 3.10.07
#define DEFAULT_IDD_WLXDISPLAYLOCKEDNOTICE_DIALOG   1900
#define DEFAULT_IDD_WLXLOGGEDONSAS_DIALOG           1950
#define DEFAULT_IDD_USER_MESSAGE_DIALOG             2500
// MSGINA control IDs
// updated by Itai Shaham 02.09.07
#define DEFAULT_IDC_WLXLOGGEDOUTSAS_USERNAME   1502
#define DEFAULT_IDC_WLXLOGGEDOUTSAS_PASSWORD   1503
#define DEFAULT_IDC_WLXLOGGEDOUTSAS_DOMAIN     1504
// updated by Rudy Kirzhner  3.10.07
#define DEFAULT_IDC_WLXLOGGEDONSAS_USERNAME   1953
#define DEFAULT_IDC_WLXLOGGEDONSAS_PASSWORD   1954
#define DEFAULT_IDC_WLXLOGGEDONSAS_DOMAIN     1956


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

//     ReadConfigValues();
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
        default:
            winlogon.reset(new WinlogonProxy<PWLX_DISPATCH_VERSION_1_3>(pWinlogonFunctions));
            break;
//         case WLX_VERSION_1_4:
//             winlogon.reset(new WinlogonProxy<PWLX_DISPATCH_VERSION_1_4>(pWinlogonFunctions));
//             break;
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


static wchar_t * getWString(const char * str) {
    int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    wchar_t * out = new wchar_t[size];
    MultiByteToWideChar(CP_UTF8, 0, str, -1, out, size);
    return out;
}


DWORD SSO::readCredentials(HWND window) {
    Log(L_DEBUG) << "About to read credentials";
    // TODO: configurable
    static const wchar_t * pipeName = L"\\\\.\\pipe\\flexvdi_creds";
    pipe = CreateFileW(pipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    return_if(pipe == INVALID_HANDLE_VALUE, pipeName, -1);

    uint8_t buffer[MAX_CRED_SIZE];
    DWORD bytesRead;
    return_if(!ReadFile(pipe, (char *)buffer, MAX_CRED_SIZE, &bytesRead, NULL),
              "Reading credentials", -1);
    CloseHandle(pipe);
    pipe = NULL;
    Log(L_DEBUG) << "Got something...";

    FlexVDICredentialsMsg * msg = (FlexVDICredentialsMsg *)buffer;
    return_if(!marshallMessage(FLEXVDI_CREDENTIALS, buffer, bytesRead),
              "Message size mismatch", -1);
    Log(L_DEBUG) << "Credentials: " << getCredentialsUser(msg) << ", "
        << getCredentialsPassword(msg);

    wchar_t * username = getWString(getCredentialsUser(msg));
    wchar_t * password = getWString(getCredentialsPassword(msg));

    PostMessage(window, WM_USER + 5, (WPARAM)username, (LPARAM)password);
    // TODO: solve racing condition with unlock
    Sleep(1000);
    PostMessage(window, WM_COMMAND, IDOK, 0);
    return 0;
}


int WINAPI SSO::WlxDialogBoxParam(HANDLE hWlx, HANDLE hInst, LPWSTR lpszTemplate,
                                  HWND hwndOwner, DLGPROC dlgprc, LPARAM dwInitParam) {
    // We only know MSGINA dialogs by identifiers.
    // In DialogBoxParam the lpTemplateName argument can be either the pointer to a null-terminated
    // character string that specifies the name of the dialog box template
    // or an integer value that specifies the resource identifier of the dialog box template.
    // If the parameter specifies a resource identifier, its high-order word must be zero
    // and its low-order word must contain the identifier.
    SSO & s = SSO::singleton();
    DLGPROC callProc = dlgprc;
    if (!HIWORD(lpszTemplate)) {
        // Hook appropriate dialog boxes as necessary.
        switch (LOWORD(lpszTemplate)) {
        case DEFAULT_IDD_WLXLOGGEDOUTSAS_DIALOG:
            s.pfWlxLoggedOutSASDlgProc = dlgprc;
            callProc = WlxLoggedOutSASDlgProc;
            break;
        case DEFAULT_IDD_WLXDISPLAYSASNOTICE_DIALOG:
            s.pfWlxDisplaySASNoticeDlgProc = dlgprc;
            callProc = WlxDisplaySASNoticeDlgProc;
            break;
        case DEFAULT_IDD_WLXDISPLAYLOCKEDNOTICE_DIALOG:
            //locked (logged on display sas)
            s.pfWlxDisplayLockedNoticeDlgProc = dlgprc;
            callProc = WlxDisplayLockedNoticeDlgProc;
            break;
        case DEFAULT_IDD_WLXLOGGEDONSAS_DIALOG:
            s.pfWlxLoggedOnSASDlgProc = dlgprc;
            callProc = WlxLoggedOnSASDlgProc;
            break;
        case DEFAULT_IDD_USER_MESSAGE_DIALOG:
            if (0) { // TODO (!GlobalConfig.m_bPresentLogonMessage) {
                s.pfWlxUserMessageDlgProc = dlgprc;
                callProc = WlxUserMessageDlgProc;
            }
            break;
        default:
            //2500  = message for user attempting logon (secpol.msc)
            //1800 = windows security
            //WCHAR buf[MAX_PATH];
            //MessageBox(0,_itow(LOWORD(lpszTemplate),buf,10),L"Dialog ID: ",MB_OK);
            break;
        }
    }

    return s.winlogon->WlxDialogBoxParam(hWlx, hInst, lpszTemplate,
                                         hwndOwner, callProc, dwInitParam);
}


BOOL CALLBACK SSO::WlxLoggedOutSASDlgProc(HWND hwndDlg, UINT uMsg,
                                          WPARAM wParam, LPARAM lParam) {
    return SSO::singleton().LogonDlgProc(hwndDlg, uMsg, wParam, lParam,
                                         SSO::singleton().pfWlxLoggedOutSASDlgProc,
                                         DEFAULT_IDC_WLXLOGGEDOUTSAS_USERNAME,
                                         DEFAULT_IDC_WLXLOGGEDOUTSAS_PASSWORD);
}


BOOL CALLBACK SSO::WlxDisplaySASNoticeDlgProc(HWND hwndDlg, UINT uMsg,
                                              WPARAM wParam, LPARAM lParam) {
    BOOL bResult = SSO::singleton().pfWlxDisplaySASNoticeDlgProc(hwndDlg, uMsg,
                                                                 wParam, lParam);
    if (uMsg == WM_INITDIALOG) {
        SSO::singleton().sendCtrlAltDel();
    }
    return bResult;
}


BOOL CALLBACK SSO::WlxDisplayLockedNoticeDlgProc(HWND hwndDlg, UINT uMsg,
                                                 WPARAM wParam, LPARAM lParam) {
    BOOL bResult = SSO::singleton().pfWlxDisplayLockedNoticeDlgProc(hwndDlg, uMsg,
                                                                    wParam, lParam);
    if (uMsg == WM_INITDIALOG) {
        SSO::singleton().sendCtrlAltDel();
    }
    return bResult;
}


BOOL CALLBACK SSO::WlxLoggedOnSASDlgProc(HWND hwndDlg, UINT uMsg,
                                         WPARAM wParam, LPARAM lParam) {
    return SSO::singleton().LogonDlgProc(hwndDlg, uMsg, wParam, lParam,
                                         SSO::singleton().pfWlxLoggedOnSASDlgProc,
                                         DEFAULT_IDC_WLXLOGGEDONSAS_USERNAME,
                                         DEFAULT_IDC_WLXLOGGEDONSAS_PASSWORD);
}


BOOL CALLBACK SSO::WlxUserMessageDlgProc(HWND hwndDlg, UINT uMsg,
                                         WPARAM wParam, LPARAM lParam) {
    BOOL bResult = SSO::singleton().pfWlxUserMessageDlgProc(hwndDlg, uMsg, wParam, lParam);

    if (uMsg == WM_INITDIALOG) {
        PostMessage(hwndDlg, WM_COMMAND, IDOK, 0);
    }
    return bResult;
}


BOOL SSO::LogonDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam,
                       DLGPROC dlgProc, int usernameIDC, int passwordIDC) {

    BOOL bResult = dlgProc(hwndDlg, uMsg, wParam, lParam);
    // check if got ok/cancel  in this dialog already
    // - if yes: credentials were sent but nothing happened (wrong password, etc.)
    //      => restart the thread and reset the condition
    if (attemptedLogin) { // TODO && GlobalConfig.m_bRestartCredentialsListener) {
        attemptedLogin = false;
        stopListening();
        thread = CreateThread(NULL, 0, &listeningThread, hwndDlg, 0, &threadId);
    }
    switch (uMsg) {
        case WM_INITDIALOG: {
            stopListening();
            thread = CreateThread(NULL, 0, &listeningThread, hwndDlg, 0, &threadId);
            break;
        }
        case WM_USER + 5: {
            // wparam should be user name and lparam should be password
            SetDlgItemTextW(hwndDlg, usernameIDC, (wchar_t *)wParam);
            SetDlgItemTextW(hwndDlg, passwordIDC, (wchar_t *)lParam);
            delete[] (wchar_t *)wParam;
            delete[] (wchar_t *)lParam;
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDOK) {
                attemptedLogin = true;
            }
        }
    }

    return bResult;
}


// void SSO::readCredentials() {
//     try {
//
//         LDB(L"SSO::readCredentials()");
//         wchar_t tempUser[MAX_CRED_SIZE];
//         wchar_t tempPassword[MAX_CRED_SIZE];
//         wchar_t tempDomain[MAX_CRED_SIZE];
//         ZeroMemory(tempUser, sizeof * tempUser);
//         ZeroMemory(tempPassword, sizeof * tempPassword);
//         ZeroMemory(tempDomain, sizeof * tempDomain);
//
//         std::wifstream fin(CRED_FILE_PATH, std::ios::binary);
//         LDB1(L"->Opened file: -%s-", CRED_FILE_PATH);
//         // apply BOM-sensitive UTF-16 facet Works IF there is a BOM
//         fin.imbue(std::locale(fin.getloc(),
//                               new std::codecvt_utf16<wchar_t, 0x10ffff,
//                               std::consume_header>));
//         // read
//         /*for(wchar_t c; fin.get(c); )
//                 std::cout << std::showbase << std::hex << c << '\n';
//         */
//         // READ THE FILE
//         int numLinea = 0;
//         while (!(fin.eof()) && numLinea < 4) {
//             std::wstring  readString;
//             std::getline(fin, readString);
//             /*std::wcout << readString<< L"; readString.length(): " << readString.length()
//                 << L"wcslen(readString.c_str()): " << wcslen(readString.c_str())
//                 << L"readString" << readString << L"\n";*/
//             switch (numLinea) {
//             case 0:
//                 wcscpy_s(tempUser, MAX_CRED_SIZE, readString.c_str());
//                 //std::wcout << L"User: " << readUser << L"wcslen: " << wcslen(readUser) << L"\n";
//                 break;
//             case 1:
//                 wcscpy_s(tempPassword, MAX_CRED_SIZE, readString.c_str());
//                 //std::wcout << L"pass: " << readPassword << L"wcslen: " << wcslen(readPassword) << L"\n";
//                 break;
//             case 2:
//                 wcscpy_s(tempDomain, MAX_CRED_SIZE, readString.c_str());
//                 //std::wcout << L"domain: " << readDomain << L"wcslen: " << wcslen(readDomain) << L"\n";
//                 break;
//             }
//             numLinea++;
//         } // end while
//
//         // STORE THE READ DATA IF THEY ARE NEW. LEAVE INTERNAL DATA UNTOUCHED OTHERWISE
//         // If we have read something different, we have new credentials
//         // Otherwise we don´t have nothing new to use
//         if (wcscmp(tempUser, flexUser) != 0 || wcscmp(tempPassword, flexPassword) != 0 || wcscmp(tempDomain, flexDomain) != 0) {
//             //LDB2(L"NEW DATA: User: -%s- Pass: -%s-", tempUser, tempPassword);
//             LDB1(L"NEW DATA: User: -%s- Pass: -********-", tempUser);
//             // Reading a user name is the bare minimum we need. (empty password and domain are possible)
//             if (wcslen(tempUser) > 0) {
//                 wcscpy_s(flexUser, MAX_CRED_SIZE, tempUser);
//                 wcscpy_s(flexPassword, MAX_CRED_SIZE, tempPassword);
//                 wcscpy_s(flexDomain, MAX_CRED_SIZE, tempDomain);
//                 usableCredentials = true;
//                 LDB(L"Setting Usable to true.");
//             } else {
//                 LDB(L"Empty readUser. Setting Usable to false.");
//                 usableCredentials = false;
//             }
//         } else {
//             LDB2(L"READ DATA IS NOT NEW: User: -%s- Dom: -%s-", tempUser, tempDomain);
//         }
//         LDB1(L"->Now Usable is %d.", usableCredentials);
//
//         fin.close();
//         LDB1(L"Closed file: -%s-", CRED_FILE_PATH);
//
//         LDB1(L"Deleting file: -%s-", CRED_FILE_PATH);
//         if (!DeleteFile(CRED_FILE_PATH)) {
//             LDB1(L"Error deleting file: %d", GetLastError());
//         } else {
//             LDB(L"Deleted");
//         }
//
//         LDB1(L"Deleting file: -%s-", SYSPREP_FILE_PATH);
//         if (!DeleteFile(SYSPREP_FILE_PATH)) {
//             LDB1(L"Error deleting file: %d", GetLastError());
//         } else {
//             LDB(L"Deleted");
//         }
//
//     } catch
//         (std::ifstream::failure e) {
//         LCF(L"Exception opening/reading/closing flex credentials file");
//         //TODO enhance exception handling
//     }
// }
