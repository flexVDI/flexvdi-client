/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <functional>
#include "Connection.hpp"
#include "VirtioPort.hpp"
#include "util.hpp"
#include "FlexVDIProto.h"

#if defined(WIN32) && defined(BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE)
#include <windows.h>
#elif defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#else
#error Do not know how to handle the Virtio port!
#endif

using namespace flexvm;
namespace asio = boost::asio;
namespace ph = std::placeholders;
using boost::system::error_code;


class VirtioPort::VirtioConnection : public StreamConnection
#ifdef WIN32
<asio::windows::stream_handle>
#else
<asio::posix::stream_descriptor>
#endif
{
public:
    VirtioConnection(asio::io_service & io, Connection::MessageHandler handler)
    : StreamConnection(io, handler) {}

    void open(const std::string & endpointName) {
        if (endpointName.empty()) return;
        std::string name = endpointName;
#ifdef WIN32
        if (name[0] != '\\')
            name = "\\\\.\\Global\\" + name;
        HANDLE portFd = ::CreateFile(toWstring(name).c_str(), GENERIC_READ | GENERIC_WRITE,
                                     0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
        throw_if(portFd == INVALID_HANDLE_VALUE, name);
#else
        if (name[0] != '/')
            name = "/dev/virtio-ports/" + name;
        int portFd = ::open(name.c_str(), O_RDWR);
        throw_if(portFd < 0, name);
#endif
        stream.assign(portFd);
        registerErrorHandler([this](const Ptr &, const error_code & error) {
            if (error == asio::error::eof) {
                Log(L_DEBUG) << "Virtio Port closed, retrying";
                // Send a reset message, the virtio port will block on write until the host
                // connects back; then, start reading again
                send(MessageBuffer(FLEXVDI_RESET, 0),
                     std::bind(&VirtioConnection::readNextMessage, this));
            }
        });
        readNextMessage();
    }
};


VirtioPort::VirtioPort(asio::io_service & io, Connection::MessageHandler handler)
    : endpointName(defaultPortName), conn(new VirtioConnection(io, handler)) {}


void VirtioPort::open() {
    conn->open(endpointName);
}


Connection::Ptr VirtioPort::spiceClient() {
    return conn;
}
