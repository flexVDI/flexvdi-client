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


void CredentialsManager::registerHandlers(DispatcherRegistry & registry) {
    registry.registerMessageHandler<FlexVDICredentialsMsg>(*this);
}


class CredentialsManager::Impl {
public:
    void open(const char * name);
    void sendMessage(const FlexVDICredentialsMsgPtr & msg);

#ifdef WIN32
    Impl(asio::io_service & io) : stream(io), io(io) {}
    static constexpr const char * defaultCredsName = "\\\\.\\pipe\\flexvdi_creds";

private:
    std::string pipeName;
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
    bool connected;
    FlexVDICredentialsMsgPtr pendingCredentials;

    void acceptConnection(const sys::error_code & error);
    void readMessage(const sys::error_code & error, std::size_t bytes_transferred);
    void sentMessage(const sys::error_code & error, std::size_t bytes_transferred);
    void listen();
    void disconnect();
};


void CredentialsManager::Impl::open(const char * name) {
#ifdef WIN32
    SECURITY_ATTRIBUTES secAttr;
    secAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    secAttr.bInheritHandle = FALSE;
    // Only current user may access the named pipe
    secAttr.lpSecurityDescriptor = NULL;
    uint32_t mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE;
    uint32_t type = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT;
    HANDLE portFd = ::CreateNamedPipeA(name, mode, type, 1, 1024, 1024, 0, &secAttr);
    throw_if(portFd == INVALID_HANDLE_VALUE, name);
    stream.assign(portFd);
#else
    ::unlink(name);
    // TODO: protect the access to the local socket
    asio::local::stream_protocol::endpoint ep(name);
    if (!acceptor.is_open())
        acceptor.open(ep.protocol());
    acceptor.bind(ep);
#endif
    connected = false;
    listen();
}


void CredentialsManager::Impl::listen() {
#ifdef WIN32
    asio::windows::overlapped_ptr overlappedPtr;
    overlappedPtr.reset(io, std::bind(&Impl::acceptConnection, this, ph::_1));
    BOOL ok = ConnectNamedPipe(stream.native_handle(), overlappedPtr.get());
    throw_if(!ok && GetLastError() != ERROR_IO_PENDING, "ConnectNamedPipe failed");
    // The operation was successfully initiated, so ownership of the
    // OVERLAPPED-derived object has passed to the io_service.
    overlappedPtr.release();
#else
    acceptor.async_accept(stream, std::bind(&Impl::acceptConnection, this, ph::_1));
#endif
}


void CredentialsManager::Impl::disconnect() {
#ifdef WIN32
    DisconnectNamedPipe(stream.native_handle());
#else
    stream.close();
#endif
    connected = false;
}


void CredentialsManager::Impl::sendMessage(const FlexVDICredentialsMsgPtr & msg) {
    // TODO: Cancel the last sending, so that connectors only receive one set of credentials
    if (connected) {
        Log(L_DEBUG) << "Connection open, sending credentials";
        std::size_t size = getCredentialsMsgSize(msg.get());
        asio::async_write(stream, asio::buffer(msg.get(), size),
                        std::bind(&Impl::sentMessage, this, ph::_1, ph::_2));
    } else {
        Log(L_DEBUG) << "Connection not open yet, pending credentials";
        pendingCredentials = msg;
    }
}


void CredentialsManager::Impl::acceptConnection(const sys::error_code & error) {
    if (error) {
        Log(L_WARNING) << "Connection failed";
        disconnect();
        listen();
    } else {
        Log(L_DEBUG) << "Connection accepted on pipe";
        connected = true;
        // TODO
        size_t messageLength = 8;
        dataBuffer.reset(new uint8_t[messageLength], std::default_delete<uint8_t[]>());
        asio::async_read(stream, asio::buffer(dataBuffer.get(), messageLength),
                         std::bind(&Impl::readMessage, this, ph::_1, ph::_2));
        if (pendingCredentials.get()) {
            Log(L_DEBUG) << "Sending pending credentials";
            sendMessage(pendingCredentials);
            pendingCredentials.reset();
        }
    }
}


void CredentialsManager::Impl::readMessage(const sys::error_code & error,
                                           std::size_t bytes_transferred) {
    if (error) {
        Log(L_DEBUG) << "Pipe closed, listening again";
        disconnect();
        listen();
    }
}


void CredentialsManager::Impl::sentMessage(const sys::error_code & error,
                                           std::size_t bytes_transferred) {
    if (error) {
        // TODO
        Log(L_WARNING) << "Sending credentials failed";
    } else {
        Log(L_DEBUG) << bytes_transferred << " bytes sent";
        pendingCredentials.reset();
    }
}


CredentialsManager::CredentialsManager(asio::io_service & io)
: endpointName(Impl::defaultCredsName), impl(nullptr) {
    impl = new Impl(io);
}


CredentialsManager::~CredentialsManager() {
    delete impl;
}


void CredentialsManager::handle(const FlexVDICredentialsMsgPtr & msg) {
    Log(L_DEBUG) << "Received a FlexVDICredentialsMsg";
    impl->sendMessage(msg);
}


void CredentialsManager::open() {
    impl->open(endpointName.c_str());
}
