/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <functional>
#include "CredentialsThread.hpp"
#include "util.hpp"
#include "FlexVDIProto.h"
using namespace flexvm;
namespace asio = boost::asio;
namespace ph = std::placeholders;
namespace sys = boost::system;


void CredentialsThread::readCredentials() {
    auto messageHandler = std::bind(&CredentialsThread::handleMessage, this, ph::_1, ph::_2);
    pipe = LocalPipeClient::create(io, messageHandler, endpointName);
    if (!pipe.get()) return;
    pipe->registerErrorHandler([&](const Connection::Ptr &, const sys::error_code & error) {
        lastError = error;
        io.stop();
    });
    Log(L_DEBUG) << "About to read credentials";
    pipe->send(MessageBuffer(FLEXVDI_ASKCREDENTIALS, 0));
    Log(L_DEBUG) << "Finished, handled " << io.run() << " events";
    io.reset();
    pipe->close();
    if (lastError)
        Log(L_ERROR) << "Error reading credentials: " << lastError.message();
}


void CredentialsThread::handleMessage(const Connection::Ptr &, const MessageBuffer & msg) {
    FlexVDIMessageHeader & header = msg.header();
    if (header.type != FLEXVDI_CREDENTIALS) {
        Log(L_ERROR) << "Expected the user credentials, received message " << header.type;
        io.stop();
        return;
    }
    uint8_t * data = msg.getMsgData();
    FlexVDICredentialsMsg * body = (FlexVDICredentialsMsg *)data;
    listener.credentialsChanged(getCredentialsUser(body),
                                getCredentialsPassword(body),
                                getCredentialsDomain(body));
    // Manually zero memory, standard algorithms have some issues
    for (std::size_t i = 0; i < header.size; ++i)
        data[i] = 0;
}


void CredentialsThread::requestCredentials() {
    thread = boost::thread(std::bind(&CredentialsThread::readCredentials, this));
}


void CredentialsThread::stop() {
    if (thread.joinable()) {
        try {
            io.stop();
            thread.join();
            io.reset();
            Log(L_DEBUG) << "Stopped credentials thread";
        } catch (std::exception & e) {
            Log(L_ERROR) << "Exception caught: " << e.what();
        }
    }
}
