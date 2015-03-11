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
    typedef asio::windows::stream_handle stream_t;

    void reset() {
        DisconnectNamedPipe(stream.native_handle());
        stream.close();
    }
    template <typename H> void listen(H acceptHandler) {
        SECURITY_ATTRIBUTES secAttr;
        secAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        secAttr.bInheritHandle = FALSE;
        // Only current user may access the named pipe
        secAttr.lpSecurityDescriptor = NULL;
        uint32_t mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
        uint32_t type = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;
        HANDLE pipeFd = ::CreateNamedPipe(toWstring(pipeName).c_str(), mode, type,
                                          PIPE_UNLIMITED_INSTANCES, 1024, 1024, 0, &secAttr);
        throw_if(pipeFd == INVALID_HANDLE_VALUE, pipeName);
        stream.assign(pipeFd);
        asio::windows::overlapped_ptr overlappedPtr;
        overlappedPtr.reset(stream.get_io_service(), acceptHandler);
        BOOL ok = ConnectNamedPipe(pipeFd, overlappedPtr.get());
        throw_if(!ok && GetLastError() != ERROR_IO_PENDING, "ConnectNamedPipe failed");
        // The operation was successfully initiated, so ownership of the
        // OVERLAPPED-derived object has passed to the io_service.
        overlappedPtr.release();
    }
    void open(const std::string & endpointName) {
        pipeName = endpointName;
        if (pipeName[0] != '\\')
            pipeName = "\\\\.\\pipe\\" + pipeName;
    }

#else
    : acceptor(io), stream(io) {}
    asio::local::stream_protocol::acceptor acceptor;
    typedef asio::local::stream_protocol::socket stream_t;

    void reset() {
        stream.close();
    }
    template <typename H> void listen(H acceptHandler) {
        acceptor.async_accept(stream, acceptHandler);
    }
    void open(const std::string & endpointName) {
        std::string name = endpointName;
        if (name[0] != '/')
            name = "/var/run/" + name;
        ::unlink(name.c_str());
        // TODO: protect the access to the local socket
        asio::local::stream_protocol::endpoint ep(name);
        acceptor.open(ep.protocol());
        acceptor.bind(ep);
        acceptor.listen();
    }

#endif
    stream_t stream;
};


class LocalPipe::LocalConnection : public StreamConnection<Listener::stream_t> {
public:
    LocalConnection(Listener::stream_t & s, Connection::MessageHandler handler)
    : StreamConnection(s.get_io_service(), handler) {
        stream = std::move(s);
    }

    void accepted() { readNextMessage(); }

#ifdef WIN32
    virtual void close() {
        DisconnectNamedPipe(stream.native_handle());
        StreamConnection<Listener::stream_t>::close();
    }
#endif
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
    listener->open(endpointName);
    listen();
}


void LocalPipe::listen() {
    listener->listen(std::bind(&LocalPipe::connectionAccepted, this, ph::_1));
}


void LocalPipe::connectionAccepted(const error_code & error) {
    if (error) {
        Log(L_WARNING) << "Local connection failed: " << error.message();
        listener->reset();
    } else {
        connections.emplace_back(new LocalConnection(listener->stream, handler));
        connections.back()->accepted();
        connections.back()->registerErrorHandler(
            [this](const Connection::Ptr & conn, const boost::system::error_code & error) {
                conn->close();
                auto it = std::find(connections.begin(), connections.end(), conn);
                if (it != connections.end()) {
                    Log(L_DEBUG) << "Removing failed connection " << conn.get();
                    connections.erase(it);
                }
            });
        Log(L_DEBUG) << "Connection " << static_cast<Connection *>(connections.back().get())
            << " accepted on pipe";
    }
    listen();
}
