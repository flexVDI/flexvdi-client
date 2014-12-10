/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <asio.hpp>
#if defined(WIN32) && defined(ASIO_HAS_WINDOWS_STREAM_HANDLE)
#include <windows.h>
#elif defined(ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#else
#error Do not know how to handle the Virtio port!
#endif

#include "util.hpp"
#include "VirtioPort.hpp"
using namespace flexvm;
using namespace asio;

struct VirtioPort::VirtioPortImpl {
#ifdef WIN32
#else
    static constexpr const char * defaultPortName = "/dev/virtio-ports/es.flexvm.agent";
    posix::stream_descriptor stream;
#endif

    VirtioPortImpl(io_service & io) : stream(io) {}
};


VirtioPort::VirtioPort(io_service & io)
: endpointName(VirtioPortImpl::defaultPortName), impl(nullptr) {
    impl = new VirtioPortImpl(io);
}


VirtioPort::~VirtioPort() {
    delete impl;
}


void VirtioPort::open() {
#ifdef WIN32
#else
    int portFd = ::open(endpointName.c_str(), O_RDWR);
    throw_if(portFd < 0, endpointName);
    impl->stream.assign(portFd);
#endif
}
