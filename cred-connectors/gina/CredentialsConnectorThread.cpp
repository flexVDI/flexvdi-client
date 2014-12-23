/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "CredentialsConnectorThread.hpp"
#include "util.hpp"
#include "FlexVDIProto.h"
using namespace flexvm;

DWORD CredentialsConnectorThread::readCredentials() {
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

    creds.setCredentials(getCredentialsUser(msg), getCredentialsPassword(msg),
                         getCredentialsDomain(msg));
    return 0;
}


void CredentialsConnectorThread::start() {
    thread = CreateThread(NULL, 0, &threadProc, this, 0, &threadId);
}


void CredentialsConnectorThread::stop() {
    if (thread != NULL) {
        TerminateThread(thread, 0);
        thread = NULL;
        threadId = 0;
        CloseHandle(pipe);
        pipe = NULL;
    }
}
