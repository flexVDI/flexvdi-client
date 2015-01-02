/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <shlwapi.h>
#include "FlexProvider.hpp"
#include "FlexCredential.hpp"
#include "util.hpp"
using namespace flexvm;


void DllAddRef();
void DllRelease();


FlexProvider::FlexProvider(HINSTANCE h) : refCount(1), dllHInst(h),
credential(nullptr), receivedCredentials(false), cpe(nullptr) {
    LogCall cl(__FUNCTION__);
    DllAddRef();
}


FlexProvider::~FlexProvider() {
    LogCall cl(__FUNCTION__);
    if (credential != nullptr) {
        credential->Release();
    }
    DllRelease();
}


HRESULT FlexProvider::QueryInterface(REFIID riid, void ** ppv) {
    if (riid == IID_ICredentialProvider) {
        ICredentialProvider * thisCP = static_cast<ICredentialProvider *>(this);
        thisCP->AddRef();
        *ppv = thisCP;
        return S_OK;
    } else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
}


// This method acts as a callback for the hardware emulator. When it's called, it simply
// tells the infrastructure that it needs to re-enumerate the credentials.
void FlexProvider::credentialsChanged(wchar_t * username, wchar_t * password,
                                      wchar_t * domain) {
    LogCall cl(__FUNCTION__);
    if (credential) {
        receivedCredentials = true;
        credential->setCredentials(username, password, domain);
        if (cpe) {
            cpe->CredentialsChanged(adviseContext);
        }
    }
}


// SetUsageScenario is the provider's cue that it's going to be asked for tiles
// in a subsequent call.
HRESULT FlexProvider::SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario,
                                        DWORD dwFlags) {
    LogCall cl(__FUNCTION__);
    UNREFERENCED_PARAMETER(dwFlags);
    HRESULT hr;
    // Decide which scenarios to support here. Returning E_NOTIMPL simply tells the caller
    // that we're not designed for that scenario.
    switch (scenario) {
    case CPUS_LOGON:
    case CPUS_UNLOCK_WORKSTATION:
        cpus = scenario;

        // Create the FlexCredential
        // We can get SetUsageScenario multiple times
        // (for example, cancel back out to the CAD screen, and then hit CAD again),
        // but there's no point in recreating our creds, since they're the same all the
        // time

        if (!credential) {
            // For the locked case, a more advanced credprov might only enumerate tiles for the
            // user whose owns the locked session, since those are the only creds that will work
            credential = new FlexCredential(dllHInst);
            if (credential) {
                // Initialize each of the object we've just created.
                // - The FlexCredential needs field descriptors.
                hr = credential->Initialize(cpus);
                if (FAILED(hr)) {
                    credential->Release();
                    credential = nullptr;
                }
                thread.reset(new ReaderThread());
                thread->Initialize(this);
            } else {
                hr = E_OUTOFMEMORY;
            }
        } else {
            //everything's already all set up
            hr = S_OK;
        }
        break;

    default:
        hr = E_NOTIMPL;
        break;
    }

    return hr;
}


// SetSerialization takes the kind of buffer that you would normally return to LogonUI for
// an authentication attempt.  It's the opposite of ICredentialProviderCredential::GetSerialization.
// GetSerialization is implement by a credential and serializes that credential.  Instead,
// SetSerialization takes the serialization and uses it to create a tile.
//
// SetSerialization is called for two main scenarios.  The first scenario is in the credui case
// where it is prepopulating a tile with credentials that the user chose to store in the OS.
// The second situation is in a remote logon case where the remote client may wish to
// prepopulate a tile with a username, or in some cases, completely populate the tile and
// use it to logon without showing any UI.
//
// If you wish to see an example of SetSerialization, please see either the SampleCredentialProvider
// sample or the SampleCredUICredentialProvider sample.  [The logonUI team says, "The original sample that
// this was built on top of didn't have SetSerialization.  And when we decided SetSerialization was
// important enough to have in the sample, it ended up being a non-trivial amount of work to integrate
// it into the main sample.  We felt it was more important to get these samples out to you quickly than to
// hold them in order to do the work to integrate the SetSerialization changes from SampleCredentialProvider
// into this sample.]
HRESULT FlexProvider::SetSerialization(
    const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION * pcpcs) {
    LogCall lc(__FUNCTION__);
    UNREFERENCED_PARAMETER(pcpcs);
    return E_NOTIMPL;
}


// Called by LogonUI to give you a callback. Providers often use the callback if they
// some event would cause them to need to change the set of tiles that they enumerated
HRESULT FlexProvider::Advise(ICredentialProviderEvents * pcpe, UINT_PTR upAdviseContext) {
    LogCall lc(__FUNCTION__);
    if (cpe) {
        cpe->Release();
    }
    cpe = pcpe;
    cpe->AddRef();
    adviseContext = upAdviseContext;
    return S_OK;
}


// Called by LogonUI when the ICredentialProviderEvents callback is no longer valid.
HRESULT FlexProvider::UnAdvise() {
    LogCall lc(__FUNCTION__);
    if (cpe) {
        cpe->Release();
        cpe = NULL;
    }
    return S_OK;
}


// Called by LogonUI to determine the number of fields in your tiles. We return the number
// of fields to be displayed on our active tile:
// FlexCredential has SFI_NUM_FIELDS fields
HRESULT FlexProvider::GetFieldDescriptorCount(DWORD * pdwCount) {
    LogCall lc(__FUNCTION__);
    *pdwCount = SFI_NUM_FIELDS;
    return S_OK;
}


static HRESULT FieldDescriptorCoAllocCopy(
    const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR & rcpfd,
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR ** ppcpfd) {

    HRESULT hr;
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR * & pcpfd = *ppcpfd;
    pcpfd = (CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR *)CoTaskMemAlloc(sizeof(**ppcpfd));
    if (pcpfd) {
        pcpfd->dwFieldID = rcpfd.dwFieldID;
        pcpfd->cpft = rcpfd.cpft;
        if (rcpfd.pszLabel) {
            hr = SHStrDupW(rcpfd.pszLabel, &pcpfd->pszLabel);
            if (FAILED(hr)) {
                CoTaskMemFree(pcpfd);
                pcpfd = NULL;
            }
        } else {
            pcpfd->pszLabel = NULL;
            hr = S_OK;
        }
    } else {
        hr = E_OUTOFMEMORY;
    }
    return hr;
}


// Gets the field descriptor for a particular field.
HRESULT FlexProvider::GetFieldDescriptorAt(DWORD dwIndex,
                                            CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR ** ppcpfd) {
    LogCall lc(__FUNCTION__);
    HRESULT hr;
    if ((dwIndex < SFI_NUM_FIELDS) && ppcpfd) {
        hr = FieldDescriptorCoAllocCopy(FlexCredential::fieldDescriptors[dwIndex], ppcpfd);
    } else {
        hr = E_INVALIDARG;
    }

    return hr;
}


// We only use one tile at any given time since the system can either be "connected" or
// "disconnected". If we decided that there were multiple valid ways to be connected with
// different sets of credentials, we would provide a combobox in the "connected" tile so
// that the user could pick which one they want to use.
// The last cred prov used gets to select the default user tile
HRESULT FlexProvider::GetCredentialCount(DWORD * pdwCount, DWORD * pdwDefault,
                                          BOOL * pbAutoLogonWithDefault) {
    LogCall cl(__FUNCTION__);
    *pdwCount = 1;
    *pdwDefault = 0;
    *pbAutoLogonWithDefault = receivedCredentials;
    receivedCredentials = false;
    return S_OK;
}


// Returns the credential at the index specified by dwIndex. This function is called
// to enumerate the tiles. Note that we need to return the right credential, which depends
// on whether we're connected or not.
HRESULT FlexProvider::GetCredentialAt(DWORD dwIndex, ICredentialProviderCredential ** ppcpc) {
    LogCall cl(__FUNCTION__);
    if (dwIndex == 0 && credential) {
        *ppcpc = static_cast<ICredentialProviderCredential *>(credential);
        credential->AddRef();
        return S_OK;
    } else {
        return E_INVALIDARG;
    }
}
