//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
//
// ReaderThread receives data through serial port
//

#pragma once

#include <windows.h>

namespace flexvm {

class CredentialConsumer {
public:
    virtual void credentialsChanged(wchar_t * username, wchar_t * password,
                                    wchar_t * domain) = 0;
};


class CReaderThread {
public:
    CReaderThread() : consumer(nullptr), thread(NULL) {}
    ~CReaderThread() { stop(); }
    HRESULT Initialize(CredentialConsumer * c);
    void stop();

private:
    CredentialConsumer * consumer;
    HANDLE thread;
    HANDLE pipe;

    BOOL ProcessNextMessage();
    static DWORD WINAPI ThreadProc(LPVOID lpParameter);
};

} // namespace flexvm
