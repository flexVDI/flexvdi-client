/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXPROVIDERFACTORY_HPP_
#define _FLEXPROVIDERFACTORY_HPP_

#include "credentialprovider.h"

namespace flexvm {

class FlexProviderFactory : public IClassFactory {
public:
    FlexProviderFactory() : refCount(1) {}

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);

    IFACEMETHODIMP_(ULONG) AddRef() {
        return InterlockedIncrement(&refCount);
    }

    IFACEMETHODIMP_(ULONG) Release() {
        LONG cRef = InterlockedDecrement(&refCount);
        if (!cRef)
            delete this;
        return cRef;
    }

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void **ppv) {
        *ppv = NULL;
        return !pUnkOuter ? createProvider(riid, ppv) : CLASS_E_NOAGGREGATION;
    }

    IFACEMETHODIMP LockServer(BOOL bLock);

    static HINSTANCE dllHInst;

private:
    virtual ~FlexProviderFactory() {}
    HRESULT createProvider(REFIID riid, void ** ppv);
    long refCount;
};

}

#endif // _FLEXPROVIDERFACTORY_HPP_
