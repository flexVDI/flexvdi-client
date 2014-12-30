/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXCREDENTIAL_HPP_
#define _FLEXCREDENTIAL_HPP_

#include <credentialprovider.h>
#include "TileFields.h"

namespace flexvm {

class CFlexCredential : public ICredentialProviderCredential {
public:
    CFlexCredential(HINSTANCE h);
    virtual ~CFlexCredential();

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

    // ICredentialProviderCredential
    IFACEMETHODIMP Advise(ICredentialProviderCredentialEvents * pcpce);
    IFACEMETHODIMP UnAdvise();
    IFACEMETHODIMP SetSelected(BOOL * pbAutoLogon);
    IFACEMETHODIMP SetDeselected();
    IFACEMETHODIMP GetFieldState(DWORD dwFieldID,
                                 CREDENTIAL_PROVIDER_FIELD_STATE * pcpfs,
                                 CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE * pcpfis);
    IFACEMETHODIMP GetStringValue(DWORD dwFieldID, PWSTR * ppwsz);
    IFACEMETHODIMP GetBitmapValue(DWORD dwFieldID, HBITMAP * phbmp);
    IFACEMETHODIMP GetCheckboxValue(DWORD dwFieldID, BOOL * pbChecked, PWSTR * ppwszLabel);
    IFACEMETHODIMP GetComboBoxValueCount(DWORD dwFieldID, DWORD * pcItems, DWORD * pdwSelectedItem);
    IFACEMETHODIMP GetComboBoxValueAt(DWORD dwFieldID, DWORD dwItem, PWSTR * ppwszItem);
    IFACEMETHODIMP GetSubmitButtonValue(DWORD dwFieldID, DWORD * pdwAdjacentTo);
    IFACEMETHODIMP SetStringValue(DWORD dwFieldID, PCWSTR pwz);
    IFACEMETHODIMP SetCheckboxValue(DWORD dwFieldID, BOOL bChecked);
    IFACEMETHODIMP SetComboBoxSelectedValue(DWORD dwFieldID, DWORD dwSelectedItem);
    IFACEMETHODIMP CommandLinkClicked(DWORD dwFieldID);
    IFACEMETHODIMP GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE * pcpgsr,
                                    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION * pcpcs,
                                    PWSTR * ppwszOptionalStatusText,
                                    CREDENTIAL_PROVIDER_STATUS_ICON * pcpsiOptionalStatusIcon);
    IFACEMETHODIMP ReportResult(NTSTATUS ntsStatus,
                                NTSTATUS ntsSubstatus,
                                PWSTR * ppwszOptionalStatusText,
                                CREDENTIAL_PROVIDER_STATUS_ICON * pcpsiOptionalStatusIcon);

    HRESULT Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus);

    void setCredentials(wchar_t * username, wchar_t * password, wchar_t * domain);

    static const FIELD_STATE_PAIR fieldStatePairs[SFI_NUM_FIELDS];
    static const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR fieldDescriptors[SFI_NUM_FIELDS];

private:
    static const int MAX_CRED_SIZE = 1024; // Max size of the data used for authenticating: user, pass, domain
    int refCount;
    HINSTANCE dllHInst;
    // The usage scenario for which we were enumerated.
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus;
    PWSTR fieldContent[SFI_NUM_FIELDS];
    CRITICAL_SECTION mutex;
    ICredentialProviderCredentialEvents * events;

    void clearPassword();
};

} // namespace flexvm

#endif // _FLEXCREDENTIAL_HPP_
