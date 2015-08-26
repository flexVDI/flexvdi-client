/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <functional>
#include "CredentialsThread.hpp"
#ifdef BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE
#include <windows.h>
#endif
#include "util.hpp"
#include "FlexVDIProto.h"
using namespace flexvm;
namespace asio = boost::asio;
namespace ph = std::placeholders;
namespace sys = boost::system;


bool CredentialsThread::openPipe() {
    std::string pipeName = endpointName;
#ifdef BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE
    if (pipeName[0] != '\\')
        pipeName = "\\\\.\\pipe\\" + pipeName;
    HANDLE h = CreateFileA(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                           OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    return_if(h == INVALID_HANDLE_VALUE, "Error opening pipe", false);
    pipe.assign(h);
#else
    sys::error_code error;
    if (pipeName[0] != '/')
        pipeName = "/var/run/" + pipeName;
    asio::local::stream_protocol::endpoint ep(pipeName);
    return_if(pipe.connect(ep, error), "Error opening pipe: " << error.message(), false);
#endif
    return true;
}


void CredentialsThread::readCredentials() {
    if (!openPipe()) return;
    Log(L_DEBUG) << "About to read credentials";
    buffer.reset((uint8_t *)new FlexVDIMessageHeader{FLEXVDI_ASKCREDENTIALS, 0},
                 [](uint8_t * p) { delete (FlexVDIMessageHeader *)p; });
    asio::async_write(pipe, asio::buffer(buffer.get(), sizeof(FlexVDIMessageHeader)),
                      std::bind(&CredentialsThread::requestSent, this, ph::_1, ph::_2));
    Log(L_DEBUG) << "Finished, handled " << io.run() << " events";
    io.reset();
    pipe.close();
    if (lastError)
        Log(L_ERROR) << "Error reading credentials: " << lastError.message();
}


void CredentialsThread::requestSent(const sys::error_code & error, std::size_t bytes) {
    lastError = error;
    if (error) return;
    asio::async_read(pipe, asio::buffer(buffer.get(), sizeof(FlexVDIMessageHeader)),
                     std::bind(&CredentialsThread::headerRead, this, ph::_1, ph::_2));
}


void CredentialsThread::headerRead(const sys::error_code & error, std::size_t bytes) {
    lastError = error;
    if (error) return;
    FlexVDIMessageHeader & header = *(FlexVDIMessageHeader *)buffer.get();
    if (header.type != FLEXVDI_CREDENTIALS) {
        Log(L_ERROR) << "Expected the user credentials, received message " << header.type;
        return;
    }
    bufferSize = header.size;
    // Manually zero memory on buffer destruction, standard algorithms have some issues
    buffer.reset(new uint8_t[bufferSize], [this](uint8_t * p) {
        for (std::size_t i = 0; i < bufferSize; ++i) p[i] = 0;
        delete[] p;
    });
    asio::async_read(pipe, asio::buffer(buffer.get(), bufferSize),
                     std::bind(&CredentialsThread::bodyRead, this, ph::_1, ph::_2));
}


void CredentialsThread::bodyRead(const boost::system::error_code & error, std::size_t bytes) {
    lastError = error;
    if (error) return;
    if (!marshallMessage(FLEXVDI_CREDENTIALS, buffer.get(), bufferSize)) {
        Log(L_ERROR) << "Credentials message size mismatch";
        return;
    }
    FlexVDICredentialsMsg * msg = (FlexVDICredentialsMsg *)buffer.get();
    listener.credentialsChanged(getCredentialsUser(msg), getCredentialsPassword(msg),
                                getCredentialsDomain(msg));
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
