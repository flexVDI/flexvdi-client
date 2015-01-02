/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXPROVIDER_HPP_
#define _FLEXPROVIDER_HPP_

#include <credentialprovider.h>
#include <memory>
#include "ReaderThread.hpp"

namespace flexvm {

class FlexCredential;
class FlexProvider : public ICredentialProvider, public CredentialConsumer {
public:
    FlexProvider(HINSTANCE h);
    virtual ~FlexProvider();

    // IUnknown
    IFACEMETHODIMP_(ULONG) AddRef() {
        return ++refCount;
    }

    IFACEMETHODIMP_(ULONG) Release() {
        LONG cRef = --refCount;
        if (!cRef) {
            delete this;
        }
        return cRef;
    }

    IFACEMETHODIMP QueryInterface(REFIID riid, void ** ppv);

    // ICredentialProvider
    IFACEMETHODIMP SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, DWORD dwFlags);
    IFACEMETHODIMP SetSerialization(const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION * pcpcs);
    IFACEMETHODIMP Advise(ICredentialProviderEvents * pcpe, UINT_PTR upAdviseContext);
    IFACEMETHODIMP UnAdvise();
    IFACEMETHODIMP GetFieldDescriptorCount(DWORD * pdwCount);
    IFACEMETHODIMP GetFieldDescriptorAt(DWORD dwIndex,
                                        CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR ** ppcpfd);
    IFACEMETHODIMP GetCredentialCount(DWORD * pdwCount, DWORD * pdwDefault,
                                      BOOL * pbAutoLogonWithDefault);
    IFACEMETHODIMP GetCredentialAt(DWORD dwIndex,
                                   ICredentialProviderCredential ** ppcpc);

    virtual void credentialsChanged(wchar_t * username, wchar_t * password, wchar_t * domain);

private:
    int refCount;
    HINSTANCE dllHInst;
    FlexCredential * credential;      // Our "connected" credential.
    std::unique_ptr<ReaderThread> thread;
    bool receivedCredentials;
    ICredentialProviderEvents * cpe;   // Used to tell our owner to re-enumerate credentials.
    UINT_PTR adviseContext;            // Used to tell our owner who we are when asking to
    // re-enumerate credentials.
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus;
};

} // namespace flexvm

#endif // _FLEXPROVIDER_HPP_
