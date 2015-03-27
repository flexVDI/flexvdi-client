/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "CredentialsManager.hpp"
#include "FlexVDIGuestAgent.hpp"
#include "util.hpp"

using namespace flexvm;


REGISTER_COMPONENT(CredentialsManager);


void CredentialsManager::sendPendingCredentials() {
    Log(L_DEBUG) << "Sending credentials";
    waitingForCredentials->send(pendingCredentials);
    waitingForCredentials.reset();
    pendingCredentials = MessageBuffer();
}


void CredentialsManager::securePendingCredentials(const FlexVDICredentialsMsgPtr & msg) {
    std::size_t size = messageSize(FLEXVDI_CREDENTIALS, (const uint8_t *) msg.get());
    uint8_t * buffer = (uint8_t *)msg.get();
    pendingCredentials = MessageBuffer(FLEXVDI_CREDENTIALS, size, [size](uint8_t * p) {
        // Manually zero memory, standard algorithms have some issues
        for (std::size_t i = 0; i < size; ++i) p[i] = 0;
        delete[] p;
    });
    std::copy_n(buffer, size, pendingCredentials.getMsgData());
    for (std::size_t i = 0; i < size; ++i)
        buffer[i] = 0;
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
        if (!first) {
            Log(L_DEBUG) << "Not the first message, discarding";
            pendingCredentials = MessageBuffer();
        }
    }
    first = false;
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

    if (pendingCredentials.isValid()) {
        sendPendingCredentials();
    } else {
        Log(L_DEBUG) << "No pending credentials, waiting";
    }
}
