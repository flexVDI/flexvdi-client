/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _CREDENTIALSMANAGER_HPP_
#define _CREDENTIALSMANAGER_HPP_

#include <memory>
#include "FlexVDIGuestAgent.hpp"

namespace flexvm {

class CredentialsManager : public FlexVDIComponent<CredentialsManager
,FlexVDICredentialsMsg
,FlexVDIAskCredentialsMsg
> {
public:
    typedef std::shared_ptr<FlexVDICredentialsMsg> FlexVDICredentialsMsgPtr;
    typedef std::shared_ptr<FlexVDIAskCredentialsMsg> FlexVDIAskCredentialsMsgPtr;

    void handle(const Connection::Ptr & src, const FlexVDICredentialsMsgPtr & msg);
    void handle(const Connection::Ptr & src, const FlexVDIAskCredentialsMsgPtr & msg);

private:
    FlexVDICredentialsMsgPtr pendingCredentials;
    Connection::Ptr waitingForCredentials;

    void sendPendingCredentials();
    void securePendingCredentials(const FlexVDICredentialsMsgPtr & msg);
};

}

#endif // _CREDENTIALSMANAGER_HPP_
