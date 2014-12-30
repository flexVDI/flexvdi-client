/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <fstream>
#include "FlexProviderFactory.hpp"
#include "CFlexProvider.h"
#include "util.hpp"
#include "guid.h"
using namespace flexvm;


static LONG g_cRef = 0;   // global dll reference count
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
static const char * getLogPath() {
    static char logPath[1024] = "c:\\flexvdi_credprov.log";
    //     char tempPath[1024];
    //     GetTempPathA(1024, tempPath);
    //     sprintf_s(logPath, 1024, "%s\\flexvdi_credprov.log", tempPath);
    return logPath;
}


STDAPI_(BOOL) DllMain(HINSTANCE hinstDll, DWORD dwReason, void *) {
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDll);
            break;
        case DLL_PROCESS_DETACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }

    logFile.open(getLogPath(), std::ios_base::app);
    logFile << std::endl << std::endl;
    Log::setLogOstream(&logFile);
    FlexProviderFactory::dllHInst = hinstDll;
    Log(L_DEBUG) << "FlexVDICredentialProvider loaded";
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
    CFlexProvider * pProvider = new CFlexProvider(dllHInst);
    if (pProvider) {
        Log(L_DEBUG) << "Created provider";
        hr = pProvider->QueryInterface(riid, ppv);
        pProvider->Release();
    } else {
        hr = E_OUTOFMEMORY;
    }
    return hr;
}
