/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _CREDENTIALSMANAGER_HPP_
#define _CREDENTIALSMANAGER_HPP_

#include <memory>
#include "FlexVDIComponent.hpp"

namespace flexvm {

class CredentialsManager : public FlexVDIComponent<CredentialsManager
,FlexVDICredentialsMsg
,FlexVDIAskCredentialsMsg
> {
public:
    typedef MessagePtr<FlexVDICredentialsMsg> FlexVDICredentialsMsgPtr;
    typedef MessagePtr<FlexVDIAskCredentialsMsg> FlexVDIAskCredentialsMsgPtr;

    void handle(const Connection::Ptr & src, const FlexVDICredentialsMsgPtr & msg);
    void handle(const Connection::Ptr & src, const FlexVDIAskCredentialsMsgPtr & msg);

    CredentialsManager() : first(true) {}

private:
    MessageBuffer pendingCredentials;
    Connection::Ptr waitingForCredentials;
    bool first;

    void sendPendingCredentials();
    void securePendingCredentials(const FlexVDICredentialsMsgPtr & msg);
};

}

#endif // _CREDENTIALSMANAGER_HPP_
