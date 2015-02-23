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


void DispatcherRegistry::handleMessage(const Connection::Ptr & src,
                                       const MessageBuffer & msgBuffer) {
    auto & header = msgBuffer.header();
    if (header.type < FLEXVDI_MAX_MESSAGE_TYPE) {
        for (auto & dispatcher : dispatchers[header.type]) {
            Log(L_DEBUG) << "Dispatching message type " << header.type
                         << " from connection " << src.get();
            dispatcher->dispatch(src, msgBuffer);
        }
    }
}


#define SET_TYPE(msg, t) \
template<> const uint32_t DispatcherRegistry::MessageType<msg>::type = t
SET_TYPE(FlexVDICredentialsMsg, FLEXVDI_CREDENTIALS);
SET_TYPE(FlexVDIAskCredentialsMsg, FLEXVDI_ASKCREDENTIALS);
SET_TYPE(FlexVDIPrintJobMsg, FLEXVDI_PRINTJOB);
SET_TYPE(FlexVDIPrintJobDataMsg, FLEXVDI_PRINTJOBDATA);
SET_TYPE(FlexVDISharePrinterMsg, FLEXVDI_SHAREPRINTER);
SET_TYPE(FlexVDIUnsharePrinterMsg, FLEXVDI_UNSHAREPRINTER);
