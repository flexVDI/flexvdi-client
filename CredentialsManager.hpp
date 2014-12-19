/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _CREDENTIALSMANAGER_HPP_
#define _CREDENTIALSMANAGER_HPP_

#include <memory>
#include <boost/asio.hpp>
#include "FlexVDIProto.h"
#include "DispatcherRegistry.hpp"

namespace flexvm {

class CredentialsManager {
public:
    typedef std::shared_ptr<FlexVDICredentialsMsg> FlexVDICredentialsMsgPtr;

    CredentialsManager(boost::asio::io_service & io);
    ~CredentialsManager();
    void handle(const FlexVDICredentialsMsgPtr & msg);
    void registerHandlers(DispatcherRegistry & registry);
    void open();
    void setEndpoint(const std::string & name) {
        endpointName = name;
    }

private:
    std::string endpointName;

    class Impl;
    Impl * impl;
};

}

#endif // _CREDENTIALSMANAGER_HPP_
