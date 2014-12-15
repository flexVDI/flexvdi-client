/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "CredentialsManager.hpp"
#include "util.hpp"

#if defined(WIN32) && defined(ASIO_HAS_WINDOWS_STREAM_HANDLE)
#include <windows.h>
#elif defined(ASIO_HAS_LOCAL_SOCKETS)
#include <unistd.h>
#endif

using namespace flexvm;

void CredentialsManager::handle(const std::shared_ptr<FlexVDICredentialsMsg> & msg) {
    Log(DEBUG) << "Received a FlexVDICredentialsMsg: " << getCredentialsUser(msg.get()) << ", "
        << getCredentialsPassword(msg.get()) << ", " << getCredentialsDomain(msg.get());
}


void CredentialsManager::registerHandlers(DispatcherRegistry & registry) {
    registry.registerMessageHandler<FlexVDICredentialsMsg>(*this);
}


class CredentialsManager::Impl {
public:
    Impl(asio::io_service & io) : acceptor(io), stream(io) {}
    void open(const char * name);

#ifdef WIN32
#else
    static constexpr const char * defaultCredsName = "/tmp/flexvm_creds";
private:
    asio::local::stream_protocol::acceptor acceptor;
    asio::local::stream_protocol::socket stream;
#endif

    std::shared_ptr<uint8_t> dataBuffer;

    void acceptConnection(const asio::error_code & error);
    void readMessage(const asio::error_code & error, std::size_t bytes_transferred);
};


void CredentialsManager::Impl::open(const char * name) {
#ifdef WIN32
#else
    ::unlink(name);
    asio::local::stream_protocol::endpoint ep(name);
    if (!acceptor.is_open())
        acceptor.open(ep.protocol());
    acceptor.bind(ep);
    using namespace std::placeholders;
    acceptor.async_accept(stream, std::bind(&Impl::acceptConnection, this, _1));
#endif
}


void CredentialsManager::Impl::acceptConnection(const asio::error_code & error) {
    if (error) {
        // TODO
    } else {
        size_t messageLength = 8;
        dataBuffer.reset(new uint8_t[messageLength], std::default_delete<uint8_t[]>());
        using namespace std::placeholders;
        asio::async_read(stream, asio::buffer(dataBuffer.get(), messageLength),
                        std::bind(&Impl::readMessage, this, _1, _2));
    }
}


void CredentialsManager::Impl::readMessage(const asio::error_code & error,
                                           std::size_t bytes_transferred) {

}


CredentialsManager::CredentialsManager(asio::io_service & io)
: endpointName(Impl::defaultCredsName), impl(nullptr) {
    impl = new Impl(io);
}


CredentialsManager::~CredentialsManager() {
    delete impl;
}


void CredentialsManager::open() {
    impl->open(endpointName.c_str());
}
