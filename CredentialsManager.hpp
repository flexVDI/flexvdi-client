/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _CREDENTIALSMANAGER_HPP_
#define _CREDENTIALSMANAGER_HPP_

#include <memory>
#include "FlexVDIProto.h"
#include "Connection.hpp"

namespace flexvm {

class CredentialsManager {
public:
    typedef std::shared_ptr<FlexVDICredentialsMsg> FlexVDICredentialsMsgPtr;

    static CredentialsManager & singleton() {
        static CredentialsManager instance;
        return instance;
    }

    void handle(const Connection::Ptr & src, const FlexVDICredentialsMsgPtr & msg);

private:
    // TODO: protect these credentials
    FlexVDICredentialsMsgPtr pendingCredentials;
};

}

#endif // _CREDENTIALSMANAGER_HPP_
