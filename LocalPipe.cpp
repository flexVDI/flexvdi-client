/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <string>
#include <functional>
#include "LocalPipe.hpp"
#include "util.hpp"

#if defined(WIN32) && defined(BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE)
#include <windows.h>
#elif defined(BOOST_ASIO_HAS_LOCAL_SOCKETS)
#include <unistd.h>
#else
#error Do not know how to handle the local pipes!
#endif

using namespace flexvm;
namespace ph = std::placeholders;
namespace asio = boost::asio;
using boost::system::error_code;


struct LocalPipe::Listener {
    Listener(asio::io_service & io)
#ifdef WIN32
    : stream(io) {}
    std::string pipeName;
    void disconnect() { DisconnectNamedPipe(stream.native_handle()); }
    typedef asio::windows::stream_handle stream_t;
#else
    : acceptor(io), stream(io) {}
    asio::local::stream_protocol::acceptor acceptor;
    void disconnect() { stream.close(); }
    typedef asio::local::stream_protocol::socket stream_t;
#endif
    stream_t stream;
};


class LocalPipe::LocalConnection : public StreamConnection<Listener::stream_t> {
public:
    LocalConnection(Listener::stream_t & s, Connection::MessageHandler handler)
    : StreamConnection(s.get_io_service(), handler) {
        stream = std::move(s);
        readNextMessage();
    }
};


LocalPipe::LocalPipe(asio::io_service & io, Connection::MessageHandler handler)
: endpointName(defaultPipeName), handler(handler), listener(new Listener(io)) {}


LocalPipe::~LocalPipe() {
    delete listener;
}


void LocalPipe::open() {
    if (endpointName.empty()) {
        Log(L_ERROR) << "The local pipe endpoint is empty";
        return;
    }
#ifdef WIN32
    listener->pipeName = endpointName;
    if (listener->pipeName[0] != '\\')
        listener->pipeName = "\\\\.\\pipe\\" + listener->pipeName;
#else
    std::string name = endpointName;
    if (name[0] != '/')
        name = "/tmp/" + name;
    ::unlink(name.c_str());
    // TODO: protect the access to the local socket
    asio::local::stream_protocol::endpoint ep(name);
    listener->acceptor.open(ep.protocol());
    listener->acceptor.bind(ep);
    listener->acceptor.listen();
#endif
    listen();
}


void LocalPipe::listen() {
    auto acceptHandler = std::bind(&LocalPipe::connectionAccepted, this, ph::_1);
#ifdef WIN32
    SECURITY_ATTRIBUTES secAttr;
    secAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    secAttr.bInheritHandle = FALSE;
    // Only current user may access the named pipe
    secAttr.lpSecurityDescriptor = NULL;
    uint32_t mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
    uint32_t type = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT;
    HANDLE pipeFd = ::CreateNamedPipeA(listener->pipeName.c_str(), mode, type,
                                       PIPE_UNLIMITED_INSTANCES, 1024, 1024, 0, &secAttr);
    throw_if(pipeFd == INVALID_HANDLE_VALUE, listener->pipeName);
    listener->stream.assign(pipeFd);
    asio::windows::overlapped_ptr overlappedPtr;
    overlappedPtr.reset(listener->stream.get_io_service(), acceptHandler);
    BOOL ok = ConnectNamedPipe(pipeFd, overlappedPtr.get());
    throw_if(!ok && GetLastError() != ERROR_IO_PENDING, "ConnectNamedPipe failed");
    // The operation was successfully initiated, so ownership of the
    // OVERLAPPED-derived object has passed to the io_service.
    overlappedPtr.release();
#else
    listener->acceptor.async_accept(listener->stream, acceptHandler);
#endif
}


void LocalPipe::connectionAccepted(const error_code & error) {
    if (error) {
        Log(L_WARNING) << "Local connection failed: " << error.message();
        listener->disconnect();
        listen();
    } else {
        Log(L_DEBUG) << "Connection accepted on pipe";
        connections.emplace_back(new LocalConnection(listener->stream, handler));
        connections.back()->registerErrorHandler(
            [this](const Connection::Ptr & conn, const boost::system::error_code & error) {
                auto it = std::find(connections.begin(), connections.end(), conn);
                if (it != connections.end()) {
                    Log(L_DEBUG) << "Removing failed connection " << conn.get();
                    connections.erase(it);
                }
            });
    }
}


// void LocalPipe::sendMessage(const FlexVDICredentialsMsgPtr & msg) {
//     // TODO: Cancel the last sending, so that connectors only receive one set of credentials
//     if (connected) {
//         Log(L_DEBUG) << "Connection open, sending credentials";
//         std::size_t size = getCredentialsMsgSize(msg.get());
//         asio::async_write(stream, asio::buffer(msg.get(), size),
//                           std::bind(&sentMessage, this, ph::_1, ph::_2));
//     } else {
//         Log(L_DEBUG) << "Connection not open yet, pending credentials";
//         pendingCredentials = msg;
//     }
// }
