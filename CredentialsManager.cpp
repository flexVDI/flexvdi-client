/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "CredentialsManager.hpp"
#include "FlexVDIGuestAgent.hpp"
#include "util.hpp"

using namespace flexvm;


COMPONENT(CredentialsManager,
          FlexVDICredentialsMsg);


void CredentialsManager::handle(const Connection::Ptr & src,
                                const FlexVDICredentialsMsgPtr & msg) {
    Log(L_DEBUG) << "Received a FlexVDICredentialsMsg from connection " << src.get();
    pendingCredentials = msg;
}


// void CredentialsManager::handle(const Connection::Ptr & src,
//                                 const FlexVDIAskCredentialsMsgPtr & msg) {
//     Log(L_DEBUG) << "Received a FlexVDIAskCredentialsMsg from connection " << src.get();
//     if (pendingCredentials) {
//         std::size_t size = getCredentialsMsgSize(pendingCredentials.get());
//         std::shared_ptr<uint8_t> buffer(pendingCredentials, (uint8_t *)pendingCredentials.get());
//         src->send(FLEXVDI_CREDENTIALS, size, buffer);
//         pendingCredentials.reset();
//     } else {
//         Log(L_DEBUG) << "No pending credentials.";
//         // TODO: save pending petitions?
//     }
// }
