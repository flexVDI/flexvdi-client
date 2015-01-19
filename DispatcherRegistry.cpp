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


void DispatcherRegistry::handleMessage(const Connection::Ptr & src, uint32_t type,
                                       const std::shared_ptr<uint8_t> & msgBuffer) {
    if (type < FLEXVDI_MAX_MESSAGE_TYPE) {
        for (auto & dispatcher : dispatchers[type]) {
            Log(L_DEBUG) << "Dispatching message type " << type << " from connection " << src.get();
            dispatcher->dispatch(src, msgBuffer);
        }
    }
}


#define SET_TYPE(msg, t) \
    template<> const uint32_t DispatcherRegistry::MessageType<msg>::type = t
SET_TYPE(FlexVDICredentialsMsg, FLEXVDI_CREDENTIALS);
