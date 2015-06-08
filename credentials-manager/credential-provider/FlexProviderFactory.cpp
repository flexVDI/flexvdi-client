/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <fstream>
#include <string>
#include "FlexProviderFactory.hpp"
#include "FlexProvider.hpp"
#include "util.hpp"
#include "guid.h"
using namespace flexvm;


static LONG g_cRef = 0;
HINSTANCE FlexProviderFactory::dllHInst = NULL;

void DllAddRef() {
    LogCall lc(__FUNCTION__);
    InterlockedIncrement(&g_cRef);
}


void DllRelease() {
    LogCall lc(__FUNCTION__);
    InterlockedDecrement(&g_cRef);
}


HRESULT FlexProviderFactory::LockServer(BOOL bLock) {
    if (bLock) {
        DllAddRef();
    } else {
        DllRelease();
    }
    return S_OK;
}


STDAPI DllCanUnloadNow() {
    LogCall lc(__FUNCTION__);
    return (g_cRef > 0) ? S_FALSE : S_OK;
}


STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    LogCall lc(__FUNCTION__);

    *ppv = NULL;
    HRESULT hr;
    if (rclsid == CLSID_flexVDI_CP) {
        FlexProviderFactory* pcf = new FlexProviderFactory();
        if (pcf) {
            Log(L_DEBUG) << "Created class factory";
            hr = pcf->QueryInterface(riid, ppv);
            pcf->Release();
        } else {
            hr = E_OUTOFMEMORY;
        }
    } else {
        Log(L_DEBUG) << "Not this class ID";
        hr = CLASS_E_CLASSNOTAVAILABLE;
    }
    return hr;
}


static std::ofstream logFile;
static const char * _getLogPath() {
    static std::string logPath;
    logPath = std::string(Log::getDefaultLogPath()) + "\\flexvdi_credprov.log";
    return logPath.c_str();
}
static const char * getLogPath() {
    static const char * logPath = _getLogPath();
    return logPath;
}


STDAPI_(BOOL) DllMain(HINSTANCE hinstDll, DWORD dwReason, void *) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDll);
            logFile.open(getLogPath(), std::ios_base::app);
            logFile << std::endl << std::endl;
            Log::setLogOstream(&logFile);
            FlexProviderFactory::dllHInst = hinstDll;
            Log(L_DEBUG) << "FlexVDICredentialProvider loaded";
            break;
        case DLL_PROCESS_DETACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}


HRESULT FlexProviderFactory::QueryInterface(REFIID riid, void ** ppv) {
    if (riid == IID_IClassFactory) {
        IClassFactory * thisCF = static_cast<IClassFactory *>(this);
        thisCF->AddRef();
        *ppv = thisCF;
        return S_OK;
    } else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
}


HRESULT FlexProviderFactory::createProvider(REFIID riid, void ** ppv) {
    HRESULT hr = E_NOINTERFACE;
    FlexProvider * pProvider = new FlexProvider(dllHInst);
    if (pProvider) {
        Log(L_DEBUG) << "Created provider";
        hr = pProvider->QueryInterface(riid, ppv);
        pProvider->Release();
    } else {
        hr = E_OUTOFMEMORY;
    }
    return hr;
}
