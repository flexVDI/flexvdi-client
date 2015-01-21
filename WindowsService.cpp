/**
 * Copyright Flexible Software Solutions S.L. 2014
 *
 * Based on vdservice.cpp, Copyright (C) 2009 Red Hat, Inc.
 */

#include <fstream>
#include "FlexVDIGuestAgent.hpp"
#include <windows.h>
#include "util.hpp"
using std::string;
using namespace flexvm;

static wchar_t serviceName[] = L"flexvdi_service";
static wchar_t serviceDescription[] =
    L"Provides several VDI functionalities on top of SPICE.";


class WindowsService {
public:
    static int install();
    static int uninstall();
    static int run();
    static const char * getLogPath();

    static WindowsService & singleton() {
        static WindowsService instance;
        return instance;
    }

private:
    SERVICE_STATUS status;
    SERVICE_STATUS_HANDLE statusHandle;

    WindowsService() {}
    void serviceMain();
    DWORD controlHandler(DWORD control, DWORD event_type,
                         LPVOID event_data, LPVOID context);

    static VOID WINAPI serviceMain_cb(DWORD argc, wchar_t * argv[]) {
        singleton().serviceMain();
    }
    static DWORD WINAPI controlHandler_cb(DWORD control, DWORD event_type,
                                          LPVOID event_data, LPVOID context) {
        return singleton().controlHandler(control, event_type, event_data, context);
    }
};


int main(int argc, char * argv[]) {
    if (argc == 2 && string("install") == argv[1]) {
        return WindowsService::install();
    } else if (argc == 2 && string("uninstall") == argv[1]) {
        return WindowsService::uninstall();
    } else {
        std::ofstream logFile(WindowsService::getLogPath(), std::ios_base::app);
        logFile << std::endl << std::endl;
        Log::setLogOstream(&logFile);
        return WindowsService::singleton().run();
    }
}


#define LOAD_ORDER_GROUP L"Pointer Port"

int WindowsService::install() {
    SC_HANDLE service_control_manager = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
    return_if(!service_control_manager, "OpenSCManager failed", 1);
    wchar_t path[_MAX_PATH + 2];
    DWORD len = GetModuleFileName(0, path + 1, _MAX_PATH);
    return_if(len == 0 || len == _MAX_PATH, "GetModuleFileName failed", 1);
    // add quotes for the case path contains a space (see CreateService doc)
    path[0] = path[len + 1] = L'\"';
    path[len + 2] = 0;
    SC_HANDLE service = CreateService(service_control_manager, serviceName,
                                      L"flexVDI Guest Agent", SERVICE_ALL_ACCESS,
                                      SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                                      SERVICE_ERROR_IGNORE, path, LOAD_ORDER_GROUP,
                                      0, 0, 0, 0);
    return_if(!service && GetLastError() != ERROR_SERVICE_EXISTS, "CreateService failed", 1);
    if (service) {
        SERVICE_DESCRIPTION descr;
        descr.lpDescription = serviceDescription;
        if (!ChangeServiceConfig2(service, SERVICE_CONFIG_DESCRIPTION, &descr)) {
            Log(L_WARNING) << "ChangeServiceConfig2 failed";
        }
        CloseServiceHandle(service);
        Log(L_DEBUG) << "Service installed successfully from " << path;
    } else {
        Log(L_WARNING) << "Service already exists";
    }
    CloseServiceHandle(service_control_manager);
    return 0;
}


int WindowsService::uninstall() {
    SC_HANDLE service_control_manager = OpenSCManager(0, 0, SC_MANAGER_CONNECT);
    return_if(!service_control_manager, "OpenSCManager failed", 1);
    SC_HANDLE service = OpenService(service_control_manager, serviceName,
                                    SERVICE_QUERY_STATUS | DELETE);
    return_if(!service, "OpenService failed", 1);
    SERVICE_STATUS status;
    return_if(!QueryServiceStatus(service, &status), "QueryServiceStatus failed", 1);
    if (status.dwCurrentState != SERVICE_STOPPED) {
        Log(L_WARNING) << "Service is still running";
    } else return_if(!DeleteService(service), "DeleteService failed", 1);
    CloseServiceHandle(service);
    CloseServiceHandle(service_control_manager);
    return 0;
}


const char * WindowsService::getLogPath() {
    static char logPath[MAX_PATH] = "c:\\flexvdi_service.log";
    if (sprintf_s(logPath, MAX_PATH, "%s\\flexvdi_service.log", Log::getDefaultLogPath()) != -1)
        return logPath;
    else
        return "c:\\flexvdi_service.log";
}


int WindowsService::run() {
    SERVICE_TABLE_ENTRY service_table[] = { {serviceName, serviceMain_cb}, {0, 0} };
    return StartServiceCtrlDispatcher(service_table);
}


#define FLEXVDI_ACCEPTED_CONTROLS \
    (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_SESSIONCHANGE)
// Sysprep file. Read by Windows during vm creation, we remove it from here on first boot.
#define SYSPREP_FILE_PATH L"A:\\sysprep.inf"

void WindowsService::serviceMain() {
    if (!SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS)) {
        Log(L_WARNING) << "SetPriorityClass failed " << GetLastError();
    }

    // Delete sysprep file
    if (!DeleteFile(SYSPREP_FILE_PATH)
        && GetLastError() != ERROR_FILE_NOT_FOUND
        && GetLastError() != ERROR_PATH_NOT_FOUND) {
        Log(L_WARNING) << "Could not delete sysprep file: (" << GetLastError() << ") "
            << std::system_category().message(GetLastError());
    }

    status.dwServiceType = SERVICE_WIN32;
    status.dwCurrentState = SERVICE_STOPPED;
    status.dwControlsAccepted = 0;
    status.dwWin32ExitCode = NO_ERROR;
    status.dwServiceSpecificExitCode = NO_ERROR;
    status.dwCheckPoint = 0;
    status.dwWaitHint = 0;
    statusHandle = RegisterServiceCtrlHandlerEx(serviceName,
                                                &WindowsService::controlHandler_cb,
                                                NULL);
    return_if(!statusHandle, "RegisterServiceCtrlHandler failed", );

    Log(L_DEBUG) << "flexVDI service starting";
    // service is starting
    status.dwCurrentState = SERVICE_START_PENDING;
    SetServiceStatus(statusHandle, &status);

    // service running
    status.dwControlsAccepted |= FLEXVDI_ACCEPTED_CONTROLS;
    status.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(statusHandle, &status);

    FlexVDIGuestAgent::singleton().run();

    Log(L_DEBUG) << "flexVDI service ending";
    // service was stopped
    status.dwCurrentState = SERVICE_STOP_PENDING;
    SetServiceStatus(statusHandle, &status);

    // service is stopped
    status.dwControlsAccepted &= ~FLEXVDI_ACCEPTED_CONTROLS;
    status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(statusHandle, &status);
}


DWORD WindowsService::controlHandler(DWORD control, DWORD event_type,
                                     LPVOID event_data, LPVOID context) {
    switch (control) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        Log(L_DEBUG) << "Stop service";
        status.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(statusHandle, &status);
        FlexVDIGuestAgent::singleton().stop();
        break;
    case SERVICE_CONTROL_INTERROGATE:
        Log(L_DEBUG) << "Interrogate service";
        SetServiceStatus(statusHandle, &status);
        break;
//     case SERVICE_CONTROL_SESSIONCHANGE: {
//         DWORD session_id = ((WTSSESSION_NOTIFICATION*)event_data)->dwSessionId;
//         vd_printf("Session %lu %s", session_id, session_events[event_type]);
//         SetServiceStatus(statusHandle, &status);
//         if (event_type == WTS_CONSOLE_CONNECT) {
//             s->_session_id = session_id;
//             s->set_control_event(VD_CONTROL_RESTART_AGENT);
//         }
//         break;
//     }
    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
    return NO_ERROR;
}
