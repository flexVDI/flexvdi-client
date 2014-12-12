/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _CREDENTIALSMANAGER_HPP_
#define _CREDENTIALSMANAGER_HPP_

#include <memory>
#include "FlexVMProto.h"
#include "DispatcherRegistry.hpp"

namespace flexvm {

class CredentialsManager {
public:
    void handle(const std::shared_ptr<FlexVMCredentialsMsg> & msg);
    void registerHandlers(DispatcherRegistry & registry);
};

}

#endif // _CREDENTIALSMANAGER_HPP_
