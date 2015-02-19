/**
 * Copyright Flexible Software Solutions S.L. 2014
 *
 * This file is adapted from RedMon,
 * Copyright (C) 1997-2012, Ghostgum Software Pty Ltd.  All rights reserved.
 *
 * Support for Windows NT <5.0 has been removed.
 */

/* redmon.c */

/*
 * This is the redirection part of the RedMon port monitor.
 *
 * This is a Port Monitor for
 *   Windows 95, 98
 *   Windows NT 3.5
 *   Windows NT 4.0
 *   Windows NT 5.0 (Windows 2000 Professional)
 *   Windows NT 5.1 (Windows XP)
 *   Windows NT 6.1 (Windows 7)
 *
 * The monitor name is "Redirected Port" .
 * A write to any port provided by this monitor will be
 * redirected to a program using a pipe to stdin.
 *
 * An example is redirecting the output from the
 * PostScript printer driver to Ghostscript, which then
 * writes to a non-PostScript printer.
 *
 * For efficiency reasons, don't use the C run time library.
 *
 * The Windows NT version must use Unicode, so Windows NT
 * specific code is conditionally compiled with a combination
 * of UNICODE, NT35, NT40 and NT50.
 */

/* Publicly accessibly functions in this Port Monitor are prefixed with
 * the following:
 *   rXXX   Windows 95 / NT3.51 / NT4
 *   rsXXX  Windows NT5 port monitor server
 *   rcXXX  Windows NT5 port monitor client (UI)
 */

#define STRICT
#include <windows.h>
#include <tchar.h>
#include <psapi.h>
#include <userenv.h>
#include <commdlg.h>
#include "redmon.h"

/* port configuration */
#define COMMANDKEY TEXT("Command")
#define LOGUSEKEY TEXT("LogFileUse")
#define LOGNAMEKEY TEXT("LogFileName")
#define LOGDEBUGKEY TEXT("LogFileDebug")
#define REDMONUSERKEY TEXT("Software\\Ghostgum\\RedMon")
struct reconfig_s {
    DWORD dwSize;   /* sizeof this structure */
    DWORD dwVersion;    /* version number of RedMon */
    TCHAR szPortName[MAXSTR];
    TCHAR szDescription[MAXSTR];
    TCHAR szCommand[MAXSTR];
    DWORD dwDelay;
    DWORD dwLogFileUse;
    TCHAR szLogFileName[MAXSTR];
    DWORD dwLogFileDebug;
};

#define PIPE_BUF_SIZE 4096
struct redata_s {
    /* Members required by all RedMon implementations */
    HANDLE hPort;       /* handle to this structure */
    HANDLE hMonitor;        /* provided by NT5.0 OpenPort */
    RECONFIG config;        /* configuration stored in registry */
    TCHAR portname[MAXSHORTSTR];    /* Name obtained during OpenPort  */

    /* Details obtained during StartDocPort  */
    TCHAR command[1024];
    TCHAR pPrinterName[MAXSTR]; /* Printer name for RedMon port */
    TCHAR pDocName[MAXSTR]; /* Document Name (from StartDocPort) */
    TCHAR pBaseName[MAXSTR];    /* Sanitised version of Document Name */
    DWORD JobId;
    TCHAR pUserName[MAXSTR];    /* User Name (from StartDocPort job info) */
    TCHAR pMachineName[MAXSTR]; /* Machine Name (from StartDocPort job info) */

    /* For running process */
    BOOL started;       /* true if process started */
    BOOL error;         /* true if process terminates early */
    HGLOBAL environment;    /* environment strings for process */
    HANDLE hChildStdinRd;
    HANDLE hChildStdinWr;   /* We write to this one */
    HANDLE hChildStdoutRd;  /* We read from this one */
    HANDLE hChildStdoutWr;
    HANDLE hChildStderrRd;  /* We read from this one */
    HANDLE hChildStderrWr;
    PROCESS_INFORMATION piProcInfo;
    /*  */
    HANDLE hLogFile;
    HANDLE hmutex;  /* To control access to pipe and file handles */
    HANDLE primary_token;   /* primary token for caller */
    TCHAR pSessionId[MAXSTR];   /* session-id for WTS support */

    /* for write thread */
    HANDLE write_event; /* To unblock write thread */
    BOOL write;     /* TRUE if write thread should keep running */
    HANDLE write_hthread;
    DWORD write_threadid;
    LPBYTE write_buffer;    /* data to write */
    DWORD write_buffer_length;  /* number of bytes of data to write */
    BOOL write_flag;        /* TRUE if WriteFile was successful */
    DWORD write_written;    /* number of bytes written */
    BYTE pipe_buf[PIPE_BUF_SIZE]; /* buffer for use in flush_stdout */
};

void write_error(REDATA * prd, DWORD err);
void write_string_to_log(REDATA * prd, LPCTSTR buf);
void redmon_cancel_job(REDATA * prd);
BOOL start_redirect(REDATA * prd);
void reset_redata(REDATA * prd);
BOOL check_process(REDATA * prd);
BOOL get_job_info(REDATA * prd);
BOOL make_env(REDATA * prd);
BOOL query_session_id(REDATA * prd);
void WriteLog(HANDLE hFile, LPCTSTR str);
void WriteError(HANDLE hFile, DWORD err);

/* we don't rely on the import library having XcvData,
 * since we may be compiling with VC++ 5.0 */
BOOL WINAPI XcvData(HANDLE hXcv, LPCWSTR pszDataName,
                    PBYTE pInputData, DWORD cbInputData,
                    PBYTE pOutputData, DWORD cbOutputData, PDWORD pcbOutputNeeded,
                    PDWORD pdwStatus);



#define PORTSNAME TEXT("Ports")
#define BACKSLASH TEXT("\\")
#define DEFAULT_DELAY 300   /* seconds */
#define MINIMUM_DELAY 15
#define PRINT_BUF_SIZE 16384

/* environment variables set for the program */
#define REDMON_PORT     TEXT("REDMON_PORT=")
#define REDMON_JOB      TEXT("REDMON_JOB=")
#define REDMON_PRINTER  TEXT("REDMON_PRINTER=")
#define REDMON_OUTPUTPRINTER  TEXT("REDMON_OUTPUTPRINTER=")
#define REDMON_MACHINE  TEXT("REDMON_MACHINE=")
#define REDMON_USER     TEXT("REDMON_USER=")
#define REDMON_DOCNAME  TEXT("REDMON_DOCNAME=")
#define REDMON_BASENAME  TEXT("REDMON_BASENAME=")
#define REDMON_FILENAME  TEXT("REDMON_FILENAME=")
#define REDMON_SESSIONID  TEXT("REDMON_SESSIONID=")
#define REDMON_TEMP     TEXT("TEMP=")
#define REDMON_TMP      TEXT("TMP=")



/* mutex used for controlling access to log file and pipe handles */
void request_mutex(REDATA * prd) {
    if ((prd->hmutex != NULL) && (prd->hmutex != INVALID_HANDLE_VALUE))
        WaitForSingleObject(prd->hmutex, 30000);
}

void release_mutex(REDATA * prd) {
    if ((prd->hmutex != NULL) && (prd->hmutex != INVALID_HANDLE_VALUE))
        ReleaseMutex(prd->hmutex);
}


/* The log file is single byte characters only */
/* Write a single character or wide character string to the log file,
 * converting it to single byte characters */
void write_string_to_log(REDATA * prd, LPCTSTR buf) {
    DWORD cbWritten;
    int count;
    CHAR cbuf[256];
    BOOL UsedDefaultChar;
    if (prd->hLogFile == INVALID_HANDLE_VALUE)
        return;

    request_mutex(prd);
    while (lstrlen(buf)) {
        count = min(lstrlen(buf), sizeof(cbuf));
        WideCharToMultiByte(CP_ACP, 0, buf, count,
                            cbuf, sizeof(cbuf), NULL, &UsedDefaultChar);
        buf += count;
        WriteFile(prd->hLogFile, cbuf, count, &cbWritten, NULL);
    }
    FlushFileBuffers(prd->hLogFile);
    release_mutex(prd);
}

void write_error(REDATA * prd, DWORD err) {
    LPVOID lpMessageBuffer;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL, err,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* user default language */
                  (LPTSTR) &lpMessageBuffer, 0, NULL);
    if (lpMessageBuffer) {
        write_string_to_log(prd, (LPTSTR)lpMessageBuffer);
        LocalFree(LocalHandle(lpMessageBuffer));
        write_string_to_log(prd, TEXT("\r\n"));
    }
}

DWORD redmon_sizeof_config(void) {
    return sizeof(RECONFIG);
}

BOOL redmon_validate_config(RECONFIG * config) {
    if (config == NULL)
        return FALSE;
    if (config->dwSize != sizeof(RECONFIG))
        return FALSE;
    if (config->dwVersion != VERSION_NUMBER)
        return FALSE;
    return TRUE;
}

LPTSTR redmon_init_config(RECONFIG * config) {
    memset(config, 0, sizeof(RECONFIG));
    config->dwSize = sizeof(RECONFIG);
    config->dwVersion = VERSION_NUMBER;
    config->dwDelay = DEFAULT_DELAY;
    config->dwLogFileDebug = FALSE;
    lstrcpyn(config->szDescription, MONITORNAME, MAXSTR - 1);
    return config->szPortName;
}

/* read the configuration from the registry */
BOOL redmon_get_config(HANDLE hMonitor, LPCTSTR portname, RECONFIG * config) {
    LONG rc = ERROR_SUCCESS;
    HKEY hkey;
    TCHAR buf[MAXSTR];
    DWORD cbData;
    DWORD dwType;
#ifdef DEBUG_REDMON
    syslog(TEXT("redmon_get_config "));
    syslog(portname);
    syslog(TEXT("\r\n"));
    syslog(TEXT("  hMonitor="));
    syshex((DWORD)hMonitor);
    syslog(TEXT("\r\n"));
#endif
    redmon_init_config(config);
    lstrcpyn(config->szPortName, portname, sizeof(config->szPortName) - 1);

    lstrcpy(buf, PORTSNAME);
    lstrcat(buf, BACKSLASH);
    lstrcat(buf, portname);
    rc = RedMonOpenKey(hMonitor, buf, KEY_READ, &hkey);
    if (rc != ERROR_SUCCESS) {
#ifdef DEBUG_REDMON
        syslog(TEXT("Failed to open registry key "));
        syslog(buf);
        syslog(TEXT("\r\n"));
#endif
        return FALSE;
    }

    cbData = sizeof(config->szDescription) - sizeof(TCHAR);
    rc = RedMonQueryValue(hMonitor, hkey, DESCKEY, &dwType,
                          (PBYTE)(config->szDescription), &cbData);
    cbData = sizeof(config->szCommand) - sizeof(TCHAR);
    rc = RedMonQueryValue(hMonitor, hkey, COMMANDKEY, &dwType,
                          (PBYTE)(config->szCommand), &cbData);
    cbData = sizeof(config->dwLogFileUse);
    rc = RedMonQueryValue(hMonitor, hkey, LOGUSEKEY, &dwType,
                          (PBYTE)(&config->dwLogFileUse), &cbData);
    cbData = sizeof(config->szLogFileName) - sizeof(TCHAR);
    rc = RedMonQueryValue(hMonitor, hkey, LOGNAMEKEY, &dwType,
                          (PBYTE)(config->szLogFileName), &cbData);
    cbData = sizeof(config->dwLogFileDebug);
    rc = RedMonQueryValue(hMonitor, hkey, LOGDEBUGKEY, &dwType,
                          (PBYTE)(&config->dwLogFileDebug), &cbData);
    RedMonCloseKey(hMonitor, hkey);
    return TRUE;
}


/* write the configuration to the registry */
BOOL redmon_set_config(HANDLE hMonitor, RECONFIG * config) {
    LONG rc = ERROR_SUCCESS;
    HKEY hkey;
    TCHAR buf[MAXSTR];

    if (!redmon_validate_config(config))
        return FALSE;

    lstrcpy(buf, PORTSNAME);
    lstrcat(buf, BACKSLASH);
    lstrcat(buf, config->szPortName);
    rc = RedMonOpenKey(hMonitor, buf, KEY_WRITE, &hkey);
    if (rc != ERROR_SUCCESS)
        return FALSE;

    if (rc == ERROR_SUCCESS)
        rc = RedMonSetValue(hMonitor, hkey, DESCKEY, REG_SZ,
                            (PBYTE)(config->szDescription),
                            sizeof(TCHAR) * (lstrlen(config->szDescription) + 1));
    if (rc == ERROR_SUCCESS)
        rc = RedMonSetValue(hMonitor, hkey, COMMANDKEY, REG_SZ,
                            (PBYTE)(config->szCommand),
                            sizeof(TCHAR) * (lstrlen(config->szCommand) + 1));
    if (rc == ERROR_SUCCESS)
        rc = RedMonSetValue(hMonitor, hkey, LOGUSEKEY, REG_DWORD,
                            (PBYTE)(&config->dwLogFileUse), sizeof(config->dwLogFileUse));
    if (rc == ERROR_SUCCESS)
        rc = RedMonSetValue(hMonitor, hkey, LOGNAMEKEY, REG_SZ,
                            (PBYTE)(config->szLogFileName),
                            sizeof(TCHAR) * (lstrlen(config->szLogFileName) + 1));
    if (rc == ERROR_SUCCESS)
        rc = RedMonSetValue(hMonitor, hkey, LOGDEBUGKEY, REG_DWORD,
                            (PBYTE)(&config->dwLogFileDebug), sizeof(config->dwLogFileDebug));
    RedMonCloseKey(hMonitor, hkey);
    return (rc == ERROR_SUCCESS);
}


void reset_redata(REDATA * prd) {
    /* do not touch prd->portname, prd->hPort or prd->hMonitor */

    prd->started = FALSE;
    prd->error = FALSE;
    prd->command[0] = '\0';
    memset(&(prd->config), 0, sizeof(prd->config));
    prd->config.dwDelay = DEFAULT_DELAY;
    prd->pPrinterName[0] = '\0';
    prd->pDocName[0] = '\0';
    prd->JobId = 0;
    prd->hChildStdinRd = INVALID_HANDLE_VALUE;
    prd->hChildStdinWr = INVALID_HANDLE_VALUE;
    prd->hChildStdoutRd = INVALID_HANDLE_VALUE;
    prd->hChildStdoutWr = INVALID_HANDLE_VALUE;
    prd->hChildStderrRd = INVALID_HANDLE_VALUE;
    prd->hChildStderrWr = INVALID_HANDLE_VALUE;
    prd->hLogFile = INVALID_HANDLE_VALUE;
    prd->piProcInfo.hProcess = INVALID_HANDLE_VALUE;
    prd->piProcInfo.hThread = INVALID_HANDLE_VALUE;
    prd->hmutex = INVALID_HANDLE_VALUE;
    prd->write = FALSE;
    prd->write_event = INVALID_HANDLE_VALUE;
    prd->write_hthread = INVALID_HANDLE_VALUE;
    prd->write_threadid = 0;
    prd->write_buffer = NULL;
    prd->write_buffer_length = 0;
    prd->primary_token = NULL;
}

/* copy stdout and stderr to log file, if open */
BOOL flush_stdout(REDATA * prd) {
    DWORD bytes_available, dwRead, dwWritten;
    BOOL result;
    BOOL got_something = FALSE;

    request_mutex(prd);

    /* copy anything on stdout to printer or log file */
    bytes_available = 0;
    result = PeekNamedPipe(prd->hChildStdoutRd, NULL, 0, NULL,
                           &bytes_available, NULL);
    while (result && bytes_available) {
        if (!ReadFile(prd->hChildStdoutRd, prd->pipe_buf, sizeof(prd->pipe_buf),
                      &dwRead, NULL) || dwRead == 0)
            break;
        got_something = TRUE;
        if (prd->hLogFile != INVALID_HANDLE_VALUE) {
            WriteFile(prd->hLogFile, prd->pipe_buf, dwRead, &dwWritten, NULL);
            FlushFileBuffers(prd->hLogFile);
        }
        result = PeekNamedPipe(prd->hChildStdoutRd, NULL, 0, NULL,
                               &bytes_available, NULL);
    }

    /* copy anything on stderr to log file */
    bytes_available = 0;
    result = PeekNamedPipe(prd->hChildStderrRd, NULL, 0, NULL,
                           &bytes_available, NULL);
    while (result && bytes_available) {
        if (!ReadFile(prd->hChildStderrRd, prd->pipe_buf, sizeof(prd->pipe_buf), &dwRead, NULL) ||
                dwRead == 0)
            break;
        got_something = TRUE;
        if (prd->hLogFile != INVALID_HANDLE_VALUE) {
            WriteFile(prd->hLogFile, prd->pipe_buf, dwRead, &dwWritten, NULL);
            FlushFileBuffers(prd->hLogFile);
        }
        result = PeekNamedPipe(prd->hChildStderrRd, NULL, 0, NULL,
                               &bytes_available, NULL);
    }

    release_mutex(prd);
    return got_something;
}


/* Check if process is running.  */
/* Return TRUE if process is running, FALSE otherwise */
/* Shut down stdin pipe if we find process has terminated */
BOOL check_process(REDATA * prd) {
    DWORD exit_status;
    if (prd->error)
        return FALSE;   /* process is not running */

    if (prd->piProcInfo.hProcess == INVALID_HANDLE_VALUE)
        prd->error = TRUE;
    if (!prd->error
            && GetExitCodeProcess(prd->piProcInfo.hProcess, &exit_status)
            && (exit_status != STILL_ACTIVE))
        prd->error = TRUE;

    if (prd->error) {
        DWORD bytes_available, dwRead;
        BOOL result;
        BYTE buf[256];
        if (prd->config.dwLogFileDebug) {
            write_string_to_log(prd,
                                TEXT("REDMON check_process: process isn't running.\r\n"));
            write_string_to_log(prd,
                                TEXT("REDMON check_process: flushing child stdin to unblock WriteThread.\r\n"));
        }
        /* flush stdin pipe to unblock WriteThread */
        bytes_available = 0;
        result = PeekNamedPipe(prd->hChildStdinRd, NULL, 0, NULL,
                               &bytes_available, NULL);
        while (result && bytes_available) {
            ReadFile(prd->hChildStdinRd, buf, sizeof(buf), &dwRead, NULL);
            result = PeekNamedPipe(prd->hChildStdinRd, NULL, 0, NULL,
                                   &bytes_available, NULL);
        }

    }
    return !prd->error;
}

/* Thread to write to stdout pipe */
DWORD WINAPI WriteThread(LPVOID lpThreadParameter) {
    HANDLE hPort = (HANDLE)lpThreadParameter;
    REDATA * prd = GlobalLock((HGLOBAL)hPort);


    if (prd == (REDATA *)NULL)
        return 1;

    if (prd->config.dwLogFileDebug)
        write_string_to_log(prd, TEXT("\r\nREDMON WriteThread: started\r\n"));

    while (prd->write && !prd->error) {
        WaitForSingleObject(prd->write_event, INFINITE);
        ResetEvent(prd->write_event);
        if (prd->write_buffer_length && prd->write_buffer) {
            if (!(prd->write_flag = WriteFile(prd->hChildStdinWr,
                                              prd->write_buffer, prd->write_buffer_length,
                                              &prd->write_written, NULL)))
                prd->write = FALSE; /* get out of here */
        }
        prd->write_buffer = NULL;
        prd->write_buffer_length = 0;
    }

    CloseHandle(prd->write_event);
    prd->write_event = INVALID_HANDLE_VALUE;

    if (prd->config.dwLogFileDebug)
        write_string_to_log(prd, TEXT("\r\nREDMON WriteThread: ending\r\n"));

    GlobalUnlock(hPort);
    return 0;
}


BOOL redmon_open_port(HANDLE hMonitor, LPTSTR pName, PHANDLE pHandle) {
    HGLOBAL hglobal = GlobalAlloc(GPTR, (DWORD)sizeof(REDATA));
    REDATA * prd = (REDATA *)GlobalLock(hglobal);
#ifdef DEBUG_REDMON
    syslog(TEXT("redmon_open_port "));
    syslog(pName);
    syslog(TEXT("\r\n"));
#endif
    if (prd == (REDATA *)NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    FillMemory((PVOID)prd, sizeof(REDATA), 0);
    reset_redata(prd);
    lstrcpy(prd->portname, pName);
    prd->hPort = hglobal;

    prd->hMonitor = hMonitor;

    /* Do the rest of the opening in rStartDocPort() */

    GlobalUnlock(hglobal);
    *pHandle = (HANDLE)hglobal;
    return TRUE;
}

BOOL redmon_close_port(HANDLE hPort) {
    /* assume files were all closed in rEndDocPort() */
#ifdef DEBUG_REDMON
    syslog(TEXT("redmon_close_port: calling ClosePort\r\n"));
#endif

    if (hPort)
        GlobalFree((HGLOBAL)hPort);

    return TRUE;
}

BOOL redmon_start_doc_port(REDATA * prd, LPTSTR pPrinterName,
                      DWORD JobId, DWORD Level, LPBYTE pDocInfo) {
    TCHAR buf[MAXSTR];
    int i;
    int j, pathsep;
    LPTSTR s;
    BOOL flag;
    HANDLE hPrinter;

    if (prd == (REDATA *)NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
#ifdef DEBUG_REDMON
    syslog(TEXT("redmon_start_doc_port:\r\n"));
    {
        TCHAR buf[1024];
        if ((Level == 1) && pDocInfo) {
            DOC_INFO_1 * dci1 = (DOC_INFO_1 *)pDocInfo;
            wsprintf(buf, TEXT("\
  Level=1\r\n\
    DocumentName=\042%s\042\r\n\
    OutputFile=\042%s\042\r\n\
    Datatype=\042%s\042\r\n"),
                     dci1->pDocName ? dci1->pDocName : TEXT("(null)"),
                     dci1->pOutputFile ? dci1->pOutputFile : TEXT("(null)"),
                     dci1->pDatatype ? dci1->pDatatype : TEXT("(null)"));
            syslog(buf);
        } else
            if ((Level == 2) && pDocInfo) {
                DOC_INFO_2 * dci2 = (DOC_INFO_2 *)pDocInfo;
                wsprintf(buf, TEXT("\
  Level=2\r\n\
    DocumentName=\042%s\042\r\n\
    OutputFile=\042%s\042\r\n\
    Datatype=\042%s\042\r\n\
    Mode=%d\r\n\
    JobId=%d\r\n"),
                         dci2->pDocName ? dci2->pDocName : TEXT("(null)"),
                         dci2->pOutputFile ? dci2->pOutputFile : TEXT("(null)"),
                         dci2->pDatatype ? dci2->pDatatype : TEXT("(null)"),
                         dci2->dwMode, dci2->JobId);
                syslog(buf);
            } else
                if ((Level == 3) && pDocInfo) {
                    DOC_INFO_3 * dci3 = (DOC_INFO_3 *)pDocInfo;
                    wsprintf(buf, TEXT("\
  Level=3\r\n\
    DocumentName=\042%s\042\r\n\
    OutputFile=\042%s\042\r\n\
    Datatype=\042%s\042\r\n\
    Flags=%d\r\n"),
                             dci3->pDocName ? dci3->pDocName : TEXT("(null)"),
                             dci3->pOutputFile ? dci3->pOutputFile : TEXT("(null)"),
                             dci3->pDatatype ? dci3->pDatatype : TEXT("(null)"),
                             dci3->dwFlags);
                    syslog(buf);
                } else {
                    wsprintf(buf, TEXT("  Level=%d pDocInfo=%d\r\n"),
                             Level, pDocInfo);
                    syslog(buf);
                }
    }
#endif

    reset_redata(prd);
    lstrcpy(prd->pPrinterName, pPrinterName);
    prd->JobId = JobId;
    /* remember document name, to be used for output job */
    if ((Level == 1) && pDocInfo) {
        DOC_INFO_1 * dci1 = (DOC_INFO_1 *)pDocInfo;
        lstrcpyn(prd->pDocName, dci1->pDocName,
                 sizeof(prd->pDocName) / sizeof(TCHAR) - 1);
    } else
        if ((Level == 2) && pDocInfo) {
            DOC_INFO_2 * dci2 = (DOC_INFO_2 *)pDocInfo;
            lstrcpyn(prd->pDocName, dci2->pDocName,
                     sizeof(prd->pDocName) / sizeof(TCHAR) - 1);
        } else
            if ((Level == 3) && pDocInfo) {
                DOC_INFO_3 * dci3 = (DOC_INFO_3 *)pDocInfo;
                lstrcpyn(prd->pDocName, dci3->pDocName,
                         sizeof(prd->pDocName) / sizeof(TCHAR) - 1);
            } else
                lstrcpy(prd->pDocName, TEXT("RedMon"));

    /* Sanitise pDocName */
    pathsep = 0;
    for (i = 0; prd->pDocName[i] != '\0'; i++) {
        if ((prd->pDocName[i] == '\\') || (prd->pDocName[i] == '/'))
            pathsep = i + 1;
    }
    j = 0;
    for (s = prd->pDocName + pathsep; *s; s++) {
        if ((*s != '<') && (*s != '>') && (*s != '\"') &&
                (*s != '|') && (*s != '/') && (*s != '\\') &&
                (*s != '?') && (*s != '*') && (*s != ':')) {
            if (*s == '.')
                break;
            prd->pBaseName[j] = *s;
            if (j < sizeof(prd->pBaseName) - 1)
                j++;
        }
    }
    prd->pBaseName[j] = '\0';

    /* Get user name and machine name from job info */
    get_job_info(prd);

    /* get configuration for this port */
    prd->config.dwSize = sizeof(prd->config);
    prd->config.dwVersion = VERSION_NUMBER;
    if (!redmon_get_config(prd->hMonitor, prd->portname, &prd->config)) {
        SetLastError(REGDB_E_KEYMISSING);
        return FALSE;   /* There are no ports */
    }

    if (prd->config.dwLogFileUse) {
        /* Open optional log file */
        prd->hLogFile = INVALID_HANDLE_VALUE;
        if (lstrlen(prd->config.szLogFileName)) {
            prd->hLogFile = CreateFile(prd->config.szLogFileName,
                                       GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL, NULL);
        }
    }

    flag = TRUE;    /* all is well */

    query_session_id(prd);

    if (!flag) {
        if (prd->hLogFile != INVALID_HANDLE_VALUE)
            ;
        CloseHandle(prd->hLogFile);
        prd->hLogFile = INVALID_HANDLE_VALUE;
        if (prd->environment)
            GlobalFree(prd->environment);
        prd->environment = NULL;
        return FALSE;
    }

    make_env(prd);

    /* Launch application */

    /* Build command line */
    lstrcpy(prd->command, TEXT("\042"));
    lstrcat(prd->command, prd->config.szCommand);
    lstrcat(prd->command, TEXT("\042 "));

    /* fix shutdown delay */
    if (prd->config.dwDelay < MINIMUM_DELAY)
        prd->config.dwDelay = MINIMUM_DELAY;

    prd->hmutex = CreateMutex(NULL, FALSE, NULL);
    if (flag)
        flag = start_redirect(prd);
    if (flag) {
        WaitForInputIdle(prd->piProcInfo.hProcess, 5000);

        /* Create thread to write to stdin pipe
         * We need this to avoid a deadlock when stdin and stdout
         * pipes are both blocked.
         */
        prd->write_event = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (prd->write_event == NULL)
            write_string_to_log(prd,
                                TEXT("couldn't create synchronization event\r\n"));
        prd->write = TRUE;
        prd->write_hthread = CreateThread(NULL, 0, &WriteThread,
                                          prd->hPort, 0, &prd->write_threadid);
    } else {
        DWORD err = GetLastError();
        /* ENGLISH */
        if (prd->environment) {
            GlobalUnlock(prd->environment);
            GlobalFree(prd->environment);
            prd->environment = NULL;
        }
        wsprintf(buf,
                 TEXT("StartDocPort: failed to start process\r\n  Port = %s\r\n  Command = %s\r\n  Error = %ld\r\n"),
                 prd->portname, prd->command, err);
        switch (err) {
        case ERROR_FILE_NOT_FOUND:
            lstrcat(buf, TEXT("  File not found\r\n"));
            break;
        case ERROR_PATH_NOT_FOUND:
            lstrcat(buf, TEXT("  Path not found\r\n"));
            break;
        case ERROR_BAD_PATHNAME:
            lstrcat(buf, TEXT("  Bad path name\r\n"));
            break;
        }
        write_string_to_log(prd, buf);
        write_error(prd, err);
    }

    if (prd->config.dwLogFileDebug) {
        wsprintf(buf,
                 TEXT("REDMON StartDocPort: returning %d\r\n\
  %s\r\n\
  Printer=%s\r\n\
  JobId=%d\r\n"),
                 flag, prd->command, prd->pPrinterName, prd->JobId);
        write_string_to_log(prd, buf);
        if ((Level == 1) && pDocInfo) {
            DOC_INFO_1 * dci1 = (DOC_INFO_1 *)pDocInfo;
            wsprintf(buf, TEXT("\
  Level=1\r\n\
    DocumentName=\042%s\042\r\n\
    OutputFile=\042%s\042\r\n\
    Datatype=\042%s\042\r\n"),
                     dci1->pDocName ? dci1->pDocName : TEXT("(null)"),
                     dci1->pOutputFile ? dci1->pOutputFile : TEXT("(null)"),
                     dci1->pDatatype ? dci1->pDatatype : TEXT("(null)"));
            write_string_to_log(prd, buf);
        } else
            if ((Level == 2) && pDocInfo) {
                DOC_INFO_2 * dci2 = (DOC_INFO_2 *)pDocInfo;
                wsprintf(buf, TEXT("\
  Level=2\r\n\
    DocumentName=\042%s\042\r\n\
    OutputFile=\042%s\042\r\n\
    Datatype=\042%s\042\r\n\
    Mode=%d\r\n\
    JobId=%d\r\n"),
                         dci2->pDocName ? dci2->pDocName : TEXT("(null)"),
                         dci2->pOutputFile ? dci2->pOutputFile : TEXT("(null)"),
                         dci2->pDatatype ? dci2->pDatatype : TEXT("(null)"),
                         dci2->dwMode, dci2->JobId);
                write_string_to_log(prd, buf);
            } else
                if ((Level == 3) && pDocInfo) {
                    DOC_INFO_3 * dci3 = (DOC_INFO_3 *)pDocInfo;
                    wsprintf(buf, TEXT("\
  Level=3\r\n\
    DocumentName=\042%s\042\r\n\
    OutputFile=\042%s\042\r\n\
    Datatype=\042%s\042\r\n\
    Flags=%d\r\n"),
                             dci3->pDocName ? dci3->pDocName : TEXT("(null)"),
                             dci3->pOutputFile ? dci3->pOutputFile : TEXT("(null)"),
                             dci3->pDatatype ? dci3->pDatatype : TEXT("(null)"),
                             dci3->dwFlags);
                    write_string_to_log(prd, buf);
                } else {
                    wsprintf(buf, TEXT("  Level=%d pDocInfo=%d\r\n"),
                             Level, pDocInfo);
                    write_string_to_log(prd, buf);
                }
        wsprintf(buf, TEXT("  delay=%d\r\n"), prd->config.dwDelay);
        write_string_to_log(prd, buf);
    }

    if (!flag) {
        /* close all file and object handles */
        if (prd->hLogFile != INVALID_HANDLE_VALUE)
            ;
        CloseHandle(prd->hLogFile);
        prd->hLogFile = INVALID_HANDLE_VALUE;

        if (prd->hChildStderrRd)
            CloseHandle(prd->hChildStderrRd);
        if (prd->hChildStderrWr)
            CloseHandle(prd->hChildStderrWr);
        if (prd->hChildStdoutRd)
            CloseHandle(prd->hChildStdoutRd);
        if (prd->hChildStdoutWr)
            CloseHandle(prd->hChildStdoutWr);
        if (prd->hChildStdinRd)
            CloseHandle(prd->hChildStdinRd);
        if (prd->hChildStdinWr)
            CloseHandle(prd->hChildStdinWr);
        prd->hChildStderrRd   = INVALID_HANDLE_VALUE;
        prd->hChildStderrWr   = INVALID_HANDLE_VALUE;
        prd->hChildStdoutRd   = INVALID_HANDLE_VALUE;
        prd->hChildStdoutWr   = INVALID_HANDLE_VALUE;
        prd->hChildStdinRd    = INVALID_HANDLE_VALUE;
        prd->hChildStdinWr    = INVALID_HANDLE_VALUE;

        if (prd->hmutex != INVALID_HANDLE_VALUE)
            CloseHandle(prd->hmutex);
        prd->hmutex    = INVALID_HANDLE_VALUE;
    }

    prd->started = flag;

    return flag;
}

void redmon_cancel_job(REDATA * prd) {
    TCHAR buf[MAXSTR];
    HANDLE hPrinter;
    if (OpenPrinter(prd->pPrinterName, &hPrinter, NULL)) {
        DWORD dwNeeded = 0;
        HGLOBAL hglobal = GlobalAlloc(GPTR, (DWORD)4096);
        JOB_INFO_1 * pjob = (JOB_INFO_1 *)GlobalLock(hglobal);
        if ((pjob != (JOB_INFO_1 *)NULL) &&
                GetJob(hPrinter, prd->JobId, 1, (LPBYTE)pjob,
                       4096, &dwNeeded)) {
            pjob->Status = JOB_STATUS_ERROR;
            SetJob(hPrinter, prd->JobId, 1, (LPBYTE)pjob,
                   JOB_CONTROL_CANCEL);
            if (prd->config.dwLogFileDebug && (prd->hLogFile != INVALID_HANDLE_VALUE)) {
                wsprintf(buf,
                         TEXT("\r\nREDMON Cancelling print job\r\n"));
                write_string_to_log(prd, buf);
            }
        } else {
            SetJob(hPrinter, prd->JobId, 0, NULL, JOB_CONTROL_CANCEL);
            if (prd->config.dwLogFileDebug && (prd->hLogFile != INVALID_HANDLE_VALUE)) {
                wsprintf(buf,
                         TEXT("\r\nREDMON Cancelling print job (second method)\r\n"));
                write_string_to_log(prd, buf);
            }
        }
        if (pjob != (JOB_INFO_1 *)NULL) {
            GlobalUnlock(hglobal);
            GlobalFree(hglobal);
        }
        ClosePrinter(hPrinter);
    }
}

/* WritePort is normally called between StartDocPort and EndDocPort,
 * but can be called outside this pair for bidirectional printers.
 */
BOOL redmon_write_port(REDATA * prd, LPBYTE  pBuffer,
                       DWORD   cbBuf, LPDWORD pcbWritten) {
    TCHAR buf[MAXSTR];
    unsigned int sleep_count;

    if (prd == (REDATA *)NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    *pcbWritten = 0;

    if (!prd->started) {
        if (prd->config.dwLogFileDebug) {
            wsprintf(buf,
                     TEXT("REDMON WritePort: called outside Start/EndDocPort, or StartDocPort get filename was cancelled.\r\n"));
            write_string_to_log(prd, buf);
        }
    }

    /* copy from output pipes to printer or log file */
    flush_stdout(prd);

    /* Make sure process is still running */
    check_process(prd);

    if (prd->error) {
        /* The process is no longer running, probably due to an error. */
        /* If we return an error from WritePort, the spooler crashes.
         * To avoid this mess, don't return an error from WritePort.
         * Instead carry on as if the error didn't occur.
         * The only evidence of an error will be the log file.
         */
        *pcbWritten = cbBuf;    /* say we wrote it all */
        if (prd->config.dwLogFileDebug
                && (prd->hLogFile != INVALID_HANDLE_VALUE)) {
            wsprintf(buf,
                     TEXT("\r\nREDMON WritePort: Process not running. \
Returning TRUE.\r\n    Ignoring %d bytes\r\n"), cbBuf);
            write_string_to_log(prd, buf);
        }

        /* Cancel the print job */
        redmon_cancel_job(prd);

        return TRUE;    /* say we wrote it all */
    }

    if (prd->config.dwLogFileDebug && (prd->hLogFile != INVALID_HANDLE_VALUE)) {
        wsprintf(buf,
                 TEXT("\r\nREDMON WritePort: about to write %d bytes to port.\r\n"),
                 cbBuf);
        write_string_to_log(prd, buf);
    }


    /* write to stdin pipe */
    prd->write_buffer_length = cbBuf;
    prd->write_buffer = pBuffer;
    prd->write_flag = TRUE;
    prd->write_written = 0;
    SetEvent(prd->write_event);
    if (prd->write && prd->write_buffer_length) {
        flush_stdout(prd);
        check_process(prd);
        Sleep(0);
    }
    sleep_count = 0;
    while (!prd->error && prd->write && prd->write_buffer_length) {
        /* wait for it to be written, while flushing stdout */
        if (flush_stdout(prd)) {
            /* We succeeded in reading something from one of the
             * pipes and the pipes are now empty.  Give up the
             * remainder of our time slice to allow the
             * other process to write something to the pipes
             * or to read from stdin.
             */
            sleep_count = 0;
            Sleep(0);
        } else
            if (prd->write_buffer_length) {
                /* The pipes were empty, and the other process
                 * hasn't finished reading stdin.
                 * Pause a little until something is available or
                 * until stdin has been read.
                 * If the process is very slow and doesn't read stdin
                 * within a reasonable time, sleep for 100ms to avoid
                 * wasting CPU.
                 */
                if (sleep_count < 10)
                    Sleep(sleep_count * 5);
                else
                    Sleep(100);
                sleep_count++;
            }
        /* Make sure process is still running */
        check_process(prd);
    }
    *pcbWritten = prd->write_written;

    if (prd->error)
        *pcbWritten = cbBuf;

    if (prd->config.dwLogFileDebug && (prd->hLogFile != INVALID_HANDLE_VALUE)) {
        DWORD cbWritten;
        request_mutex(prd);
        WriteFile(prd->hLogFile, pBuffer, cbBuf, &cbWritten, NULL);
        FlushFileBuffers(prd->hLogFile);
        release_mutex(prd);
        wsprintf(buf,
                 TEXT("\r\nREDMON WritePort: %s  count=%d written=%d\r\n"),
                 (prd->write_flag ? TEXT("OK") : TEXT("Failed")),
                 cbBuf, *pcbWritten);
        write_string_to_log(prd, buf);
    }

    flush_stdout(prd);

    if (prd->error) {
        if (prd->config.dwLogFileDebug
                && (prd->hLogFile != INVALID_HANDLE_VALUE)) {
            wsprintf(buf,
                     TEXT("\r\nREDMON WritePort: Process not running. \
Returning TRUE.\r\n    Ignoring %d bytes\r\n"), cbBuf);
            write_string_to_log(prd, buf);
        }
        /* Cancel the print job */
        redmon_cancel_job(prd);
    }

    return TRUE;    /* returning FALSE crashes Win95 spooler */
}

/* ReadPort can be called within a Start/EndDocPort pair,
 * and also outside this pair.
 */
BOOL redmon_read_port(REDATA * prd, LPBYTE pBuffer,
                      DWORD  cbBuffer, LPDWORD pcbRead) {
    /* we don't support reading */
    *pcbRead = 0;

    if (prd == (REDATA *)NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    Sleep(1000);    /* Pause a little */

    if (prd->config.dwLogFileDebug) {
        TCHAR buf[MAXSTR];
        wsprintf(buf, TEXT("REDMON ReadPort: returning FALSE. Process %s\r\n"),
                 prd->error ? TEXT("has an ERROR.") : TEXT("is OK."));
        write_string_to_log(prd, buf);
        wsprintf(buf, TEXT("REDMON ReadPort: You must disable bi-directional printer support for this printer\r\n"));
        write_string_to_log(prd, buf);
    }

    /* We don't support read port */
    return FALSE;
}

BOOL redmon_end_doc_port(REDATA * prd) {
    TCHAR buf[MAXSTR];
    DWORD exit_status;
    HANDLE hPrinter;
    unsigned int i;

    if (prd == (REDATA *)NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
#ifdef DEBUG_REDMON
    syslog(TEXT("redmon_end_doc_port\r\n"));
#endif

    if (prd->config.dwLogFileDebug)
        write_string_to_log(prd,
                            TEXT("REDMON EndDocPort: starting\r\n"));

    /* tell write thread to shut down */
    prd->write_buffer_length = 0;
    prd->write_buffer = NULL;
    prd->write = FALSE;
    SetEvent(prd->write_event);
    Sleep(0);   /* let write thread terminate */

    for (i = 0; i < 20; i++) {
        if (GetExitCodeThread(prd->write_hthread, &exit_status)) {
            if (exit_status != STILL_ACTIVE)
                break;
        } else
            break;
        Sleep(50);
    }
    CloseHandle(prd->write_hthread);

    /* Close stdin to signal EOF */
    if (prd->hChildStdinWr != INVALID_HANDLE_VALUE)
        CloseHandle(prd->hChildStdinWr);
    prd->hChildStdinWr  = INVALID_HANDLE_VALUE;

    flush_stdout(prd);

    /* wait here for up to 'delay' seconds until process ends */
    /* so that process has time to write stdout/err */
    exit_status = 0;
    for (i = 0; i < prd->config.dwDelay; i++) {
        if (prd->piProcInfo.hProcess == INVALID_HANDLE_VALUE)
            break;
        if (GetExitCodeProcess(prd->piProcInfo.hProcess, &exit_status)) {
            if (exit_status != STILL_ACTIVE)
                break;
        } else {
            /* process doesn't exist */
            break;
        }

        flush_stdout(prd);

        Sleep(1000);
    }

    flush_stdout(prd);

    if (prd->config.dwLogFileDebug) {
        wsprintf(buf,
                 TEXT("REDMON EndDocPort: process %s after %d second%s\r\n"),
                 (exit_status == STILL_ACTIVE) ?
                 TEXT("still running") : TEXT("finished"),
                 i, (i != 1) ? TEXT("s") : TEXT(""));
        write_string_to_log(prd, buf);
    }

    /* if process still running, we might want to kill it
     * with TerminateProcess(), but since this can have bad
     * side effects, don't do it yet.
     */

    /* copy anything on stdout/err to log file */
    flush_stdout(prd);


    /* NT documentation says *we* should cancel the print job. */
    /* 95 documentation says nothing about this. */
    if (OpenPrinter(prd->pPrinterName, &hPrinter, NULL)) {
        SetJob(hPrinter, prd->JobId, 0, NULL, JOB_CONTROL_CANCEL);
        ClosePrinter(hPrinter);
    }

    /* Close all handles */

    if (prd->environment) {
        GlobalUnlock(prd->environment);
        GlobalFree(prd->environment);
        prd->environment = NULL;
    }

    if (prd->primary_token != NULL) {
        CloseHandle(prd->primary_token);
        prd->primary_token = NULL;
    }

    if (prd->config.dwLogFileDebug)
        write_string_to_log(prd,
                            TEXT("REDMON EndDocPort: ending\r\n"));

    if (prd->hLogFile != INVALID_HANDLE_VALUE)
        CloseHandle(prd->hLogFile);

    if ((prd->hmutex != NULL) && (prd->hmutex != INVALID_HANDLE_VALUE))
        CloseHandle(prd->hmutex);

    if (prd->piProcInfo.hProcess != INVALID_HANDLE_VALUE)
        CloseHandle(prd->piProcInfo.hProcess);

    if (prd->piProcInfo.hThread != INVALID_HANDLE_VALUE)
        CloseHandle(prd->piProcInfo.hThread);

    /* Close the last of the pipes */
    if (prd->hChildStderrRd)
        CloseHandle(prd->hChildStderrRd);
    if (prd->hChildStdoutRd)
        CloseHandle(prd->hChildStdoutRd);

    /* These should alerady be closed */
    if (prd->hChildStderrWr)
        CloseHandle(prd->hChildStderrWr);
    if (prd->hChildStdoutWr)
        CloseHandle(prd->hChildStdoutWr);
    if (prd->hChildStdinRd)
        CloseHandle(prd->hChildStdinRd);
    if (prd->hChildStdinWr)
        CloseHandle(prd->hChildStdinWr);


    reset_redata(prd);

    return TRUE;
}

BOOL redmon_set_port_timeouts(REDATA * prd, LPCOMMTIMEOUTS lpCTO, DWORD reserved) {
    /* Do nothing */

    if (prd == (REDATA *)NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

#ifdef DEBUG_REDMON
    {
        TCHAR buf[MAXSTR];
        if (prd->config.dwLogFileDebug) {
            wsprintf(buf, TEXT("REDMON SetPortTimeOuts: returning TRUE\r\n\
	    values = %d %d %d %d %d\r\n"),
                     lpCTO->ReadIntervalTimeout,
                     lpCTO->ReadTotalTimeoutMultiplier,
                     lpCTO->ReadTotalTimeoutConstant,
                     lpCTO->WriteTotalTimeoutMultiplier,
                     lpCTO->WriteTotalTimeoutConstant);
            write_string_to_log(prd, buf);
        }
    }
#endif

    return TRUE;
}


/* Get the directory for temporary files */
/* Store the result in buf */
/* Don't copy if length is greater than len characters */
BOOL get_temp(LPTSTR value, int len) {
    TCHAR buf[256];
    DWORD dwLength;
    /* If we run from the Windows NT SYSTEM account, many
     * environment variables aren't set.  We need to look
     * in several places to find the name of a directory
     * for temporary files.
     */

    if (!len)
        return FALSE;
    value[0] = '\0';

    dwLength = GetEnvironmentVariable(TEXT("TEMP"), value, len);
    if (dwLength == 0)
        dwLength = GetEnvironmentVariable(TEXT("TMP"), value, len);

    if (dwLength == 0) {
        HKEY hkey;
        DWORD cbData, keytype;
        value[0] = '\0';
        if (RegOpenKeyEx(HKEY_USERS, TEXT(".DEFAULT\\Environment"), 0,
                         KEY_ALL_ACCESS, &hkey) == ERROR_SUCCESS) {
            keytype = REG_SZ;
            buf[0] = '\0';
            cbData = sizeof(buf);
            if (RegQueryValueEx(hkey, TEXT("TEMP"), 0, &keytype,
                                (LPBYTE)buf, &cbData) == ERROR_SUCCESS)
                dwLength = cbData;
            if (dwLength == 0) {
                buf[0] = '\0';
                cbData = sizeof(buf);
                if (RegQueryValueEx(hkey, TEXT("TMP"), 0, &keytype,
                                    (LPBYTE)buf, &cbData) == ERROR_SUCCESS)
                    dwLength = cbData;
            }
            RegCloseKey(hkey);
            if (dwLength) {
                /* Replace %USERPROFILE%, %SystemDrive% etc. */
                dwLength = ExpandEnvironmentStrings(buf, value, len);
            }
        }
    }

    if (dwLength == 0)
        GetWindowsDirectory(value, len);
    if (dwLength)
        return TRUE;
    return FALSE;
}

BOOL get_job_info(REDATA * prd) {
    HGLOBAL hPrinter;
    if (OpenPrinter(prd->pPrinterName, &hPrinter, NULL)) {
        DWORD dwNeeded = 0;
        HGLOBAL hglobal = GlobalAlloc(GPTR, (DWORD)4096);
        JOB_INFO_1 * pjob = (JOB_INFO_1 *)GlobalLock(hglobal);
        if ((pjob != (JOB_INFO_1 *)NULL) &&
                GetJob(hPrinter, prd->JobId, 1, (LPBYTE)pjob,
                       4096, &dwNeeded)) {
            lstrcpyn(prd->pMachineName, pjob->pMachineName,
                     sizeof(prd->pMachineName) / sizeof(TCHAR) - 1);
            lstrcpyn(prd->pUserName, pjob->pUserName,
                     sizeof(prd->pUserName) / sizeof(TCHAR) - 1);
        }
        if (pjob != (JOB_INFO_1 *)NULL) {
            GlobalUnlock(hglobal);
            GlobalFree(hglobal);
        }
        ClosePrinter(hPrinter);
        return TRUE;
    }
    return FALSE;
}

/* return length of env in characters (which may not be bytes) */
int env_length(LPTSTR env) {
    LPTSTR p;
    p = env;
    while (*p) {
        while (*p)
            p++;
        p++;
    }
    p++;
    return (p - env);
}

/* join two environment blocks together, returning a handle
 * to a new environment block.  The caller must free this with
 * GlobalFree() when it is finished.
 */
HGLOBAL join_env(LPTSTR env1, LPTSTR env2) {
    int len1, len2;
    HGLOBAL henv;
    LPTSTR env;
    len1 = env_length(env1);    /* length in characters, not bytes */
    len2 = env_length(env2);
    henv = GlobalAlloc(GPTR, (DWORD)((len1 + len2) * sizeof(TCHAR)));
    env = GlobalLock(henv);
    if (env == NULL)
        return NULL;
    MoveMemory(env, env1, len1 * sizeof(TCHAR));
    env += len1 - 1;
    MoveMemory(env, env2, len2 * sizeof(TCHAR));
    GlobalUnlock(henv);
    return henv;
}

/* Append a string to an environment block.
 * It is assumed that the environment block has sufficient
 * space.
 *  env is the environment block.
 *  name is the environment variable name, which includes a trailing '='.
 *  len is the length of name in bytes, including the trailing null.
 *  value is the environment variable value.
 */
void append_env(LPTSTR env, LPTSTR name, int len, LPTSTR value) {
    int oldlen;
    oldlen = env_length(env);
    env = env + oldlen - 1;
    MoveMemory(env, name, len);
    env += len / sizeof(TCHAR) - 1;
    MoveMemory(env, value, lstrlen(value)*sizeof(TCHAR));
    env += lstrlen(value) + 1;
    *env = '\0';
}

/* create an environment variable block which contains
 * some RedMon extras about the print job.
 */
HGLOBAL make_job_env(REDATA * prd) {
    int len;
    TCHAR buf[32];
    TCHAR temp[256];
    HGLOBAL henv;
    LPTSTR env;
    BOOL bTEMP = FALSE;
    BOOL bTMP = FALSE;

    wsprintf(buf, TEXT("%d"), prd->JobId);
    len = sizeof(REDMON_PORT) +
          sizeof(REDMON_JOB) +
          sizeof(REDMON_OUTPUTPRINTER) +
          sizeof(REDMON_MACHINE) +
          sizeof(REDMON_USER) +
          sizeof(REDMON_DOCNAME) +
          sizeof(REDMON_BASENAME) +
          sizeof(REDMON_SESSIONID) +
          (lstrlen(prd->portname) +
           lstrlen(buf) + 1 +
           lstrlen(prd->pPrinterName) + 1 +
           lstrlen(prd->pMachineName) + 1 +
           lstrlen(prd->pUserName) + 1 +
           lstrlen(prd->pDocName) + 1 +
           lstrlen(prd->pBaseName) + 1 +
           lstrlen(prd->pSessionId) + 1 +
           1) * sizeof(TCHAR);

    if (GetEnvironmentVariable(TEXT("TEMP"), temp,
                               sizeof(temp) / sizeof(TCHAR) - 1) == 0)
        bTEMP = TRUE;   /* Need to define TEMP */
    if (GetEnvironmentVariable(TEXT("TMP"), temp,
                               sizeof(temp) / sizeof(TCHAR) - 1) == 0)
        bTMP = TRUE;    /* Need to define TMP */
    get_temp(temp, sizeof(temp) / sizeof(TCHAR));
    if (bTEMP)
        len += sizeof(REDMON_TEMP) + (lstrlen(temp) + 1) * sizeof(TCHAR);
    if (bTMP)
        len += sizeof(REDMON_TMP) + (lstrlen(temp) + 1) * sizeof(TCHAR);

    henv = GlobalAlloc(GPTR, len);
    env = GlobalLock(henv);
    if (env == NULL)
        return NULL;
    append_env(env, REDMON_PORT, sizeof(REDMON_PORT), prd->portname);
    append_env(env, REDMON_JOB, sizeof(REDMON_JOB), buf);
    append_env(env, REDMON_PRINTER, sizeof(REDMON_PRINTER), prd->pPrinterName);
    append_env(env, REDMON_MACHINE, sizeof(REDMON_MACHINE), prd->pMachineName);
    append_env(env, REDMON_USER, sizeof(REDMON_USER), prd->pUserName);
    append_env(env, REDMON_DOCNAME, sizeof(REDMON_DOCNAME), prd->pDocName);
    append_env(env, REDMON_BASENAME, sizeof(REDMON_BASENAME), prd->pBaseName);
    append_env(env, REDMON_SESSIONID, sizeof(REDMON_SESSIONID), prd->pSessionId);
    if (bTEMP)
        append_env(env, REDMON_TEMP, sizeof(REDMON_TEMP), temp);
    if (bTMP)
        append_env(env, REDMON_TMP, sizeof(REDMON_TMP), temp);
    GlobalUnlock(henv);
    return henv;
}



/* write contents of the environment block to the log file */
void dump_env(REDATA * prd, LPTSTR env) {
    LPTSTR name, next;
    write_string_to_log(prd, TEXT("Environment:\r\n  "));
    next = env;
    while (*next) {
        name = next;
        while (*next)
            next++;
        write_string_to_log(prd, name);
        write_string_to_log(prd, TEXT("\r\n  "));
        next++;
    }
    write_string_to_log(prd, TEXT("\r\n"));
}

BOOL make_env(REDATA * prd) {
    LPTSTR env_block = NULL;
    LPTSTR env_strings = NULL;
    LPTSTR env = NULL;
    LPTSTR extra_env;
    BOOL destroy_env = FALSE;
    HGLOBAL h_extra_env;
    /* Add some environment variables */
    /* It would be simpler to use SetEnvironmentVariable()
     * and then GetEnvironmentStrings(), then to delete
     * the environment variables we added, but this could
     * cause problems if two RedMon ports simultaneously
     * did this.
     */


    /* If environment already exists, then we created it
     * before asking for filename.
     * Delete and recreate it to update REDMON_FILENAME.
     */
    if (prd->environment) {
        GlobalUnlock(prd->environment);
        GlobalFree(prd->environment);
        prd->environment = NULL;
    }

    if (env == NULL)
        env = env_strings = GetEnvironmentStrings();

    h_extra_env = make_job_env(prd);
    extra_env = GlobalLock(h_extra_env);
    prd->environment = join_env(env, extra_env);
    GlobalUnlock(h_extra_env);
    GlobalFree(h_extra_env);

    if (env_block)
        DestroyEnvironmentBlock(env_block);
    if (env_strings)
        FreeEnvironmentStrings(env_strings);

    if (prd->hLogFile != INVALID_HANDLE_VALUE) {
        env = GlobalLock(prd->environment);
        dump_env(prd, env);
        GlobalUnlock(prd->environment);
    }
    return TRUE;
}

/* Query session id from a primary token obtained from
 * the impersonation token of the thread.
 * Session id is needed to identify client a WTS session.
 */
BOOL query_session_id(REDATA * prd) {
    BOOL fRet = TRUE;
    HANDLE htoken = NULL, hduptoken = NULL;
    TCHAR buf[MAXSTR];

    /* get impersonation token */
    if (!(fRet = OpenThreadToken(GetCurrentThread() ,
                                 TOKEN_DUPLICATE | TOKEN_IMPERSONATE,
                                 TRUE, &htoken))) {
        DWORD err = GetLastError();
        wsprintf(buf, TEXT("OpenThreadToken failed, error code=%d\r\n"),
                 err);
        write_string_to_log(prd, buf);
        write_error(prd, err);
        lstrcpy(prd->pSessionId, TEXT("0"));
    }

    if (fRet) {
        /* Duplicate it to create a primary token */
        if (!(fRet = DuplicateTokenEx(htoken, TOKEN_ALL_ACCESS, NULL,
                                      SecurityImpersonation, TokenPrimary, &hduptoken))) {
            DWORD err = GetLastError();
            wsprintf(buf, TEXT("DuplicateTokenEx failed, error code=%d\r\n"),
                     err);
            write_string_to_log(prd, buf);
            write_error(prd, err);
            lstrcpy(prd->pSessionId, TEXT("0"));
        }
        CloseHandle(htoken);
    }

    if (fRet) {
        DWORD dwRetLen = 0;
        DWORD dwSessionId = 0;
        /* query session-id from token */
        fRet = GetTokenInformation(hduptoken, TokenSessionId,
                                   &dwSessionId, sizeof(DWORD), &dwRetLen);
        if (fRet)
            wsprintf(prd->pSessionId, TEXT("%ld"), dwSessionId);
        CloseHandle(hduptoken);
    }

    return (fRet);
}


/* start_redirect() was originally based on an example in the Win32 SDK
 * which used GetStdHandle() and SetStdHandle() to redirect stdio.
 * The example works under Windows 95, but not under NT.
 * For NT, we need to use
 *  siStartInfo.dwFlags = STARTF_USESTDHANDLES;
 *  siStartInfo.hStdInput = prd->hChildStdinRd;
 *  siStartInfo.hStdOutput = prd->hChildStdoutWr;
 *  siStartInfo.hStdError = prd->hChildStderrWr;
 * The SDK example does NOT include these.  Most strange for an
 * example that was written before Windows 95 existed!
 * STARTF_USESTDHANDLES also works for Windows 95, so the original
 * code is commented out with #ifdef SAVESTD / #endif
 */

/* Start child program with redirected standard input and output */
BOOL start_redirect(REDATA * prd) {
    SECURITY_ATTRIBUTES saAttr;
    STARTUPINFO siStartInfo;
    HANDLE hPipeTemp;
    LPVOID env;

    /* Set the bInheritHandle flag so pipe handles are inherited. */
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    /* Create anonymous inheritable pipes for STDIN, STDOUT and STDERR
     * for child. For each pipe, create a noninheritable duplicate handle
     * of our end of the pipe, then close the inheritable handle.
     */
    if (!CreatePipe(&prd->hChildStdinRd, &hPipeTemp, &saAttr, 0))
        return FALSE;
    if (!DuplicateHandle(GetCurrentProcess(), hPipeTemp,
                         GetCurrentProcess(), &prd->hChildStdinWr, 0,
                         FALSE,       /* not inherited */
                         DUPLICATE_SAME_ACCESS)) {
        CloseHandle(hPipeTemp);
        return FALSE;
    }
    CloseHandle(hPipeTemp);

    if (!CreatePipe(&hPipeTemp, &prd->hChildStdoutWr, &saAttr, 0))
        return FALSE;   /* cleanup of pipes will occur in caller */
    if (!DuplicateHandle(GetCurrentProcess(), hPipeTemp,
                         GetCurrentProcess(), &prd->hChildStdoutRd, 0,
                         FALSE,       /* not inherited */
                         DUPLICATE_SAME_ACCESS)) {
        CloseHandle(hPipeTemp);
        return FALSE;
    }
    CloseHandle(hPipeTemp);

    if (!CreatePipe(&hPipeTemp, &prd->hChildStderrWr, &saAttr, 0))
        return FALSE;
    if (!DuplicateHandle(GetCurrentProcess(), hPipeTemp,
                         GetCurrentProcess(), &prd->hChildStderrRd, 0,
                         FALSE,       /* not inherited */
                         DUPLICATE_SAME_ACCESS)) {
        CloseHandle(hPipeTemp);
        return FALSE;
    }
    CloseHandle(hPipeTemp);

    /* Now create the child process. */

    /* Set up members of STARTUPINFO structure. */

    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.lpReserved = NULL;
    siStartInfo.lpDesktop = NULL;
    siStartInfo.lpTitle = NULL;  /* use executable name as title */
    siStartInfo.dwX = siStartInfo.dwY = CW_USEDEFAULT;      /* ignored */
    siStartInfo.dwXSize = siStartInfo.dwYSize = CW_USEDEFAULT;  /* ignored */
    siStartInfo.dwXCountChars = 80;
    siStartInfo.dwYCountChars = 25;
    siStartInfo.dwFillAttribute = 0;            /* ignored */
    siStartInfo.dwFlags = STARTF_USESTDHANDLES;
    siStartInfo.wShowWindow = SW_SHOWNORMAL;        /* ignored */
    siStartInfo.cbReserved2 = 0;
    siStartInfo.lpReserved2 = NULL;
    siStartInfo.hStdInput = prd->hChildStdinRd;
    siStartInfo.hStdOutput = prd->hChildStdoutWr;
    siStartInfo.hStdError = prd->hChildStderrWr;

    if (prd->environment)
        env = GlobalLock(prd->environment);
    else
        env = NULL;

    /* Create the child process. */
    if (!CreateProcess(NULL,
                       prd->command,  /* command line                       */
                       NULL,          /* process security attributes        */
                       NULL,          /* primary thread security attributes */
                       TRUE,          /* handles are inherited              */
                       CREATE_UNICODE_ENVIRONMENT,  /* creation flags       */
                       env,           /* environment                        */
                       NULL,          /* use parent's current directory     */
                       &siStartInfo,  /* STARTUPINFO pointer                */
                       &prd->piProcInfo))  /* receives PROCESS_INFORMATION  */
        return FALSE;

    /* After process creation, restore the saved STDIN and STDOUT. */

    /* We now close our copy of the inheritable pipe handles.
     * The other process still contains a copy of the pipe handle,
     * so the pipe will stay open until the other end closes their
     * handle.
     */
    if (prd->hChildStdinRd != INVALID_HANDLE_VALUE)
        CloseHandle(prd->hChildStdinRd);
    prd->hChildStdinRd = INVALID_HANDLE_VALUE;
    if (prd->hChildStdoutWr != INVALID_HANDLE_VALUE)
        CloseHandle(prd->hChildStdoutWr);
    prd->hChildStdoutWr = INVALID_HANDLE_VALUE;
    if (prd->hChildStderrWr != INVALID_HANDLE_VALUE)
        CloseHandle(prd->hChildStderrWr);
    prd->hChildStderrWr = INVALID_HANDLE_VALUE;

    return TRUE;
}

void WriteLog(HANDLE hFile, LPCTSTR str) {
    DWORD dwBytesWritten;
    WriteFile(hFile, str,
              lstrlen(str) * sizeof(TCHAR), &dwBytesWritten, NULL);
}

void WriteError(HANDLE hFile, DWORD err) {
    LPVOID lpMessageBuffer;
    DWORD dwBytesWritten = 0;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL, err,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* user default language */
                  (LPTSTR) &lpMessageBuffer, 0, NULL);
    if (lpMessageBuffer) {
        WriteFile(hFile, (LPTSTR)lpMessageBuffer,
                  lstrlen(lpMessageBuffer) * sizeof(TCHAR),
                  &dwBytesWritten, NULL);
        LocalFree(LocalHandle(lpMessageBuffer));
    }
}

