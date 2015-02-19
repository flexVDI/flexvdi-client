/**
 * Copyright Flexible Software Solutions S.L. 2014
 *
 * This file is adapted from RedMon,
 * Copyright (C) 1997-2012, Ghostgum Software Pty Ltd.  All rights reserved.
 *
 * Support for Windows NT <5.0 has been removed.
 */

#include <winspool.h>
#include <winsplp.h>
#ifndef __BORLANDC__
#define _export
#endif
#ifdef _MSC_VER
#define PORTMONEXPORT __declspec(dllexport)
#else
#define PORTMONEXPORT
#endif

#ifdef _WIN64
# define DLGRETURN INT_PTR
# define HOOKRETURN UINT_PTR
# define REDLONGPTR LONG_PTR
#else
# define DLGRETURN BOOL
# define HOOKRETURN UINT
# ifdef GWLP_USERDATA
#  define REDLONGPTR LONG_PTR
# else
#  define REDLONGPTR LONG
#  define GWLP_USERDATA GWL_USERDATA
#  define GetWindowLongPtr GetWindowLong
#  define SetWindowLongPtr SetWindowLong
#  define SetWindowLongPtr SetWindowLong
# endif
#endif

#define MONITORNAME TEXT("flexVDI Redirection Port")
#define VERSION_MAJOR_NUMBER 1
#define VERSION_MINOR_NUMBER 90
#define VERSION_NUMBER VERSION_MAJOR_NUMBER * 100 + VERSION_MINOR_NUMBER
#define MAXSTR 512
#define MAXSHORTSTR 64
#define DESCKEY TEXT("Description")


typedef struct redata_s REDATA;
typedef struct reconfig_s RECONFIG;

/* to write a log file for debugging RedMon, uncomment the following line */
/*
#define DEBUG_REDMON
*/


/***********************************************************************/
/* Functions defined in portmon.c */

LONG RedMonOpenKey(HANDLE hMonitor, LPCTSTR pszSubKey, REGSAM samDesired,
    PHKEY phkResult);
LONG RedMonCloseKey(HANDLE hMonitor, HANDLE hcKey);
LONG RedMonEnumKey(HANDLE hMonitor, HANDLE hcKey, DWORD dwIndex,
    LPTSTR pszName, PDWORD pcchName);
LONG RedMonCreateKey(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszSubKey,
    DWORD dwOptions, REGSAM samDesired,
    PSECURITY_ATTRIBUTES pSecurityAttributes,
    PHKEY phckResult, PDWORD pdwDisposition);
LONG RedMonDeleteKey(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszSubKey);
LONG RedMonSetValue(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszValue,
    DWORD dwType, const BYTE* pData, DWORD cbData);
LONG RedMonQueryValue(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszValue,
        PDWORD pType, PBYTE pData, PDWORD pcbData);

void show_help(HWND hwnd, int id);

/* for debugging */
void syslog(LPCTSTR buf);
void syserror(DWORD err);
void syshex(DWORD num);
void sysnum(DWORD num);

/***********************************************************************/
/* External functions needed by portmon.c */

/* read the configuration from the registry */
BOOL redmon_get_config(HANDLE hMonitor, LPCTSTR portname, RECONFIG *config);
/* write the configuration to the registry */
BOOL redmon_set_config(HANDLE hMonitor, RECONFIG *config);
DWORD redmon_sizeof_config(void);
BOOL redmon_validate_config(RECONFIG *config);
/* initialise the config structure and return a pointer to the port name */
LPTSTR redmon_init_config(RECONFIG *config);

void reset_redata(REDATA *prd);

/* Implementation of port monitor functions */
BOOL redmon_open_port(HANDLE hMonitor, LPTSTR pName, PHANDLE pHandle);
BOOL redmon_close_port(HANDLE hPort);
BOOL redmon_start_doc_port(REDATA *prd, LPTSTR pPrinterName,
        DWORD JobId, DWORD Level, LPBYTE pDocInfo);
BOOL redmon_write_port(REDATA *prd, LPBYTE  pBuffer,
        DWORD   cbBuf, LPDWORD pcbWritten);
BOOL redmon_read_port(REDATA *prd, LPBYTE pBuffer,
        DWORD  cbBuffer, LPDWORD pcbRead);
BOOL redmon_end_doc_port(REDATA *prd);
BOOL redmon_set_port_timeouts(REDATA *prd, LPCOMMTIMEOUTS lpCTO,
        DWORD reserved);
