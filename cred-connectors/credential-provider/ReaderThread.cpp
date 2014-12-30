//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//

#include "ReaderThread.h"
#include "FlexVDIProto.h"
#include "util.hpp"
using namespace flexvm;


HRESULT CReaderThread::Initialize(CredentialConsumer * c) {
    HRESULT hr = S_OK;
    consumer = c;
    // Create and launch the thread.
    thread = CreateThread(NULL, 0, ThreadProc, this, 0, NULL);
    if (!thread) {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }
    return hr;
}


void CReaderThread::stop() {
    if (thread != NULL) {
        TerminateThread(thread, 0);
        thread = NULL;
        CloseHandle(pipe);
        pipe = NULL;
    }
}


DWORD WINAPI CReaderThread::ThreadProc(LPVOID lpParameter) {
    CReaderThread *pReaderThread = static_cast<CReaderThread *>(lpParameter);
    while (pReaderThread->ProcessNextMessage()) {}
    return 0;
}


static wchar_t * getUnicodeString(const char * s) {
    size_t newsize = strlen(s) + 1;
    wchar_t * wcstring = (wchar_t *)CoTaskMemAlloc(newsize * sizeof(wchar_t));
    size_t convertedChars = 0;
    mbstowcs_s(&convertedChars, wcstring, newsize, s, _TRUNCATE);
    return wcstring;
}


BOOL CReaderThread::ProcessNextMessage() {
    static const int MAX_CRED_SIZE = 1024;

    Log(L_DEBUG) << "About to read credentials";
    // TODO: configurable
    static const char * pipeName = "\\\\.\\pipe\\flexvdi_creds";
    pipe = CreateFileA(pipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    return_if(pipe == INVALID_HANDLE_VALUE, "Error opening pipe", -1);

    uint8_t buffer[MAX_CRED_SIZE];
    DWORD bytesRead;
    BOOL success = ReadFile(pipe, (char *)buffer, MAX_CRED_SIZE, &bytesRead, NULL);
    CloseHandle(pipe);
    pipe = NULL;
    return_if(!success, "Reading credentials", -1);

    FlexVDICredentialsMsg * msg = (FlexVDICredentialsMsg *)buffer;
    return_if(!marshallMessage(FLEXVDI_CREDENTIALS, buffer, bytesRead),
              "Message size mismatch", -1);
    Log(L_DEBUG) << "Credentials received";

    consumer->credentialsChanged(getUnicodeString(getCredentialsUser(msg)),
                                 getUnicodeString(getCredentialsPassword(msg)),
                                 getUnicodeString(getCredentialsDomain(msg)));

    SecureZeroMemory(buffer, bytesRead);
    return TRUE;
}
