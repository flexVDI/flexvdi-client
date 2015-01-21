/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "CredentialsManager.hpp"
#include "util.hpp"

using namespace flexvm;


REGISTER_COMPONENT(CredentialsManager);


void CredentialsManager::sendPendingCredentials() {
    Log(L_DEBUG) << "Sending credentials";
    std::size_t size = getCredentialsMsgSize(pendingCredentials.get());
    std::shared_ptr<uint8_t> buffer(pendingCredentials, (uint8_t *)pendingCredentials.get());
    waitingForCredentials->send(FLEXVDI_CREDENTIALS, size, buffer);
    waitingForCredentials.reset();
    pendingCredentials.reset();
}


void CredentialsManager::securePendingCredentials(const FlexVDICredentialsMsgPtr & msg) {
    std::size_t size = getCredentialsMsgSize(msg.get());
    uint8_t * buffer = (uint8_t *)msg.get();
    std::shared_ptr<uint8_t> secureBuffer;
    secureBuffer.reset(new uint8_t[size], [size](uint8_t * p) {
        // Manually zero memory, standard algorithms have some issues
        for (std::size_t i = 0; i < size; ++i) p[i] = 0;
        delete[] p;
    });
    std::copy_n(buffer, size, secureBuffer.get());
    for (std::size_t i = 0; i < size; ++i)
        buffer[i] = 0;
    pendingCredentials = FlexVDICredentialsMsgPtr(secureBuffer,
                                                  (FlexVDICredentialsMsg *)secureBuffer.get());
}


void CredentialsManager::handle(const Connection::Ptr & src,
                                const FlexVDICredentialsMsgPtr & msg) {
    Log(L_DEBUG) << "Received a FlexVDICredentialsMsg from connection " << src.get();
    securePendingCredentials(msg);
    if (waitingForCredentials && waitingForCredentials->isOpen()) {
        sendPendingCredentials();
    } else {
        waitingForCredentials.reset();
        Log(L_DEBUG) << "No process waiting for credentials";
    }
}


void CredentialsManager::handle(const Connection::Ptr & src,
                                const FlexVDIAskCredentialsMsgPtr & msg) {
    Log(L_DEBUG) << "Received a FlexVDIAskCredentialsMsg from connection " << src.get();

    if (waitingForCredentials && waitingForCredentials->isOpen()) {
        // Only one process waiting for credentials allowed
        src->close();
        return;
    } else {
        waitingForCredentials = src;
    }

    if (pendingCredentials) {
        sendPendingCredentials();
    } else {
        Log(L_DEBUG) << "No pending credentials, waiting";
    }
}
