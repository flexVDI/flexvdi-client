/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <functional>
#include "VirtioPort.hpp"
#include "util.hpp"
#include "FlexVDIProto.h"

#if defined(WIN32) && defined(ASIO_HAS_WINDOWS_STREAM_HANDLE)
#include <windows.h>
#elif defined(ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#else
#error Do not know how to handle the Virtio port!
#endif

using namespace flexvm;

class VirtioPort::Impl {
public:
    Impl(asio::io_service & io, Handler & handler) : stream(io), handler(handler) {}
    void open(const char * name);
    void readNextHeader() {
        using namespace std::placeholders;
        asio::async_read(stream, asio::buffer(&header, sizeof(FlexVDIMessageHeader)),
                         std::bind(&Impl::readHeader, this, _1, _2));
    }
    void readNextMessage(std::size_t messageLength) {
        dataBuffer.reset(new uint8_t[messageLength], std::default_delete<uint8_t[]>());
        using namespace std::placeholders;
        asio::async_read(stream, asio::buffer(dataBuffer.get(), messageLength),
                         std::bind(&Impl::readMessage, this, _1, _2));
    }

#ifdef WIN32
#else
    static constexpr const char * defaultPortName = "/dev/virtio-ports/es.flexvm.agent";
private:
    asio::posix::stream_descriptor stream;
#endif

    Handler & handler;
    FlexVDIMessageHeader header;
    std::shared_ptr<uint8_t> dataBuffer;

    void readHeader(const asio::error_code & error, std::size_t bytes_transferred);
    void readMessage(const asio::error_code & error, std::size_t bytes_transferred);
};


void VirtioPort::Impl::open(const char * name) {
#ifdef WIN32
#else
    int portFd = ::open(name, O_RDWR);
    throw_if(portFd < 0, name);
    stream.assign(portFd);
#endif
    readNextHeader();
}


void VirtioPort::Impl::readHeader(const asio::error_code & error,
                                  std::size_t bytes_transferred) {
    if (error) {
        Log(WARNING) << "Error reading virtio message header";
        // TODO: Closed?
    } else {
        marshallHeader(&header);
        Log(DEBUG) << "Read header for message type " << header.type << " and size " << header.size;
        readNextMessage(header.size);
    }
}


void VirtioPort::Impl::readMessage(const asio::error_code & error,
                                   std::size_t bytes_transferred) {
    if (error) {
        Log(WARNING) << "Error reading virtio message";
        // TODO: Closed?
    } else {
        // TODO Is bytes_transferred == header.size ??
        if (!marshallMessage(header.type, dataBuffer.get(), bytes_transferred)) {
            Log(WARNING) << "Message size mismatch reading virtio message";
        } else {
            handler.handle(header.type, dataBuffer);
        }
        readNextHeader();
    }
}


VirtioPort::VirtioPort(asio::io_service & io, Handler & handler)
: endpointName(Impl::defaultPortName), impl(nullptr) {
    impl = new Impl(io, handler);
}


VirtioPort::~VirtioPort() {
    delete impl;
}


void VirtioPort::open() {
    impl->open(endpointName.c_str());
}
