/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "FlexVDIProto.h"
#include "DispatcherRegistry.hpp"
#include "util.hpp"
using namespace flexvm;

DispatcherRegistry::DispatcherRegistry()
: dispatchers(new std::list<std::unique_ptr<BaseMessageDispatcher>>[FLEXVDI_MAX_MESSAGE_TYPE])
{}


bool DispatcherRegistry::dispatch(uint32_t type, const std::shared_ptr<uint8_t> & msgBuffer) {
    bool dispatched = false;
    if (type < FLEXVDI_MAX_MESSAGE_TYPE) {
        for (auto & dispatcher : dispatchers[type]) {
            Log(L_DEBUG) << "Dispatching message type " << type;
            dispatcher->dispatch(msgBuffer);
            dispatched = true;
        }
    }
    return dispatched;
}

#define SET_TYPE(msg, t) \
    template<> const uint32_t DispatcherRegistry::MessageType<msg>::type = t
SET_TYPE(FlexVDICredentialsMsg, FLEXVDI_CREDENTIALS);
