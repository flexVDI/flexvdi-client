/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "CredentialsManager.hpp"
#include "util.hpp"

#if defined(WIN32) && defined(BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE)
#include <windows.h>
#elif defined(BOOST_ASIO_HAS_LOCAL_SOCKETS)
#include <unistd.h>
#endif

using namespace flexvm;
namespace asio = boost::asio;
namespace sys = boost::system;
namespace ph = std::placeholders;

void CredentialsManager::handle(const std::shared_ptr<FlexVDICredentialsMsg> & msg) {
    Log(L_DEBUG) << "Received a FlexVDICredentialsMsg: " << getCredentialsUser(msg.get()) << ", "
        << getCredentialsPassword(msg.get()) << ", " << getCredentialsDomain(msg.get());
}


void CredentialsManager::registerHandlers(DispatcherRegistry & registry) {
    registry.registerMessageHandler<FlexVDICredentialsMsg>(*this);
}


class CredentialsManager::Impl {
public:
    void open(const char * name);

#ifdef WIN32
    Impl(asio::io_service & io) : stream(io), io(io) {}
    static constexpr const char * defaultCredsName = "\\\\.\\pipe\\flexvdi_creds";

private:
    asio::windows::stream_handle stream;
#else
    Impl(asio::io_service & io) : acceptor(io), stream(io), io(io) {}
    static constexpr const char * defaultCredsName = "/tmp/flexvdi_creds";

private:
    asio::local::stream_protocol::acceptor acceptor;
    asio::local::stream_protocol::socket stream;
#endif

    asio::io_service & io;
    std::shared_ptr<uint8_t> dataBuffer;

    void acceptConnection(const sys::error_code & error);
    void readMessage(const sys::error_code & error, std::size_t bytes_transferred);
};


void CredentialsManager::Impl::open(const char * name) {
#ifdef WIN32
    uint32_t type = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE |
                    PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
    HANDLE portFd = ::CreateNamedPipe(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, type,
                                      PIPE_UNLIMITED_INSTANCES, 1024, 1024, 0, nullptr);
    throw_winerror_if(portFd == INVALID_HANDLE_VALUE, name);
    stream.assign(portFd);
    asio::windows::overlapped_ptr overlappedPtr;
    overlappedPtr.reset(io, std::bind(&Impl::acceptConnection, this, ph::_1));
    BOOL ok = ConnectNamedPipe(portFd, overlappedPtr.get());
    throw_winerror_if(!ok && GetLastError() != ERROR_IO_PENDING, "ConnectNamedPipe failed");
    // The operation was successfully initiated, so ownership of the
    // OVERLAPPED-derived object has passed to the io_service.
    overlappedPtr.release();
#else
    ::unlink(name);
    asio::local::stream_protocol::endpoint ep(name);
    if (!acceptor.is_open())
        acceptor.open(ep.protocol());
    acceptor.bind(ep);
    acceptor.async_accept(stream, std::bind(&Impl::acceptConnection, this, ph::_1));
#endif
}


void CredentialsManager::Impl::acceptConnection(const sys::error_code & error) {
    if (error) {
        // TODO
    } else {
        size_t messageLength = 8;
        dataBuffer.reset(new uint8_t[messageLength], std::default_delete<uint8_t[]>());
        asio::async_read(stream, asio::buffer(dataBuffer.get(), messageLength),
                         std::bind(&Impl::readMessage, this, ph::_1, ph::_2));
    }
}


void CredentialsManager::Impl::readMessage(const sys::error_code & error,
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
