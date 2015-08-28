/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifdef WIN32

#include <windows.h>
#include <winspool.h>
#include <cwchar>


class WindowsLibrary {
public:
    WindowsLibrary(const wchar_t * name) {
        libHandle = LoadLibrary(name);
    }
    ~WindowsLibrary() {
        if (libHandle)
            FreeLibrary(libHandle);
    }

    bool isOpen() const {
        return libHandle != nullptr;
    }
    FARPROC getProcAddress(const char * name) const {
        return GetProcAddress(libHandle, name);
    }

private:
    HMODULE libHandle;
};


// MinGw version of XcvDataW seems broken, we load it dynamically
extern "C" WINBOOL WINAPI XcvDataW(HANDLE hXcv, PCWSTR pszDataName, PBYTE pInputData,
                                   DWORD cbInputData, PBYTE pOutputData, DWORD cbOutputData,
                                   PDWORD pcbOutputNeeded, PDWORD pdwStatus) {
    typedef decltype(&XcvDataW) Proc;
    WindowsLibrary spool(L"Winspool.drv");
    if (spool.isOpen()) {
        Proc fp = (Proc)spool.getProcAddress("XcvDataW");
        if (!fp) {
            return FALSE;
        } else
            return fp(hXcv, pszDataName, pInputData, cbInputData,
                      pOutputData, cbOutputData, pcbOutputNeeded, pdwStatus);
    }
    return FALSE;
}


// And SetDefaultPrinterW seems missing, but only for 32-bit ???
extern "C" WINBOOL WINAPI SetDefaultPrinterW(LPCWSTR pszPrinter) {
    typedef decltype(&SetDefaultPrinterW) Proc;
    WindowsLibrary spool(L"Winspool.drv");
    if (spool.isOpen()) {
        Proc fp = (Proc)spool.getProcAddress("SetDefaultPrinterW");
        if (!fp) {
            return FALSE;
        } else
            return fp(pszPrinter);
    }
    return FALSE;
}


extern "C" WINBOOL WINAPI DeletePrinterDriverExW(LPWSTR pName, LPWSTR pEnvironment,
                                                 LPWSTR pDriverName, DWORD dwDeleteFlag,
                                                 DWORD dwVersionFlag) {
    typedef decltype(&DeletePrinterDriverExW) Proc;
    WindowsLibrary spool(L"Winspool.drv");
    if (spool.isOpen()) {
        Proc fp = (Proc)spool.getProcAddress("DeletePrinterDriverExW");
        if (!fp) {
            return FALSE;
        } else
            return fp(pName, pEnvironment, pDriverName, dwDeleteFlag, dwVersionFlag);
    }
    return FALSE;
}

#endif // WIN32
