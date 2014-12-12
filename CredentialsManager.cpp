/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "CredentialsManager.hpp"
#include "util.hpp"
using namespace flexvm;

void CredentialsManager::handle(const std::shared_ptr<FlexVMCredentialsMsg> & msg) {
    Log(DEBUG) << "Received a FlexVMCredentialsMsg: " << getCredentialsUser(msg.get()) << ", "
        << getCredentialsPassword(msg.get()) << ", " << getCredentialsDomain(msg.get());
}

void CredentialsManager::registerHandlers(DispatcherRegistry & registry) {
    registry.registerMessageHandler<FlexVMCredentialsMsg>(*this);
}
