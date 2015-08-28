/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "LocalPipeClient.hpp"
#include "util.hpp"

#if defined(WIN32) && defined(BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE)
#include <windows.h>
#elif defined(BOOST_ASIO_HAS_LOCAL_SOCKETS)
#include <unistd.h>
#else
#error Do not know how to handle the local pipes!
#endif

using namespace flexvm;

bool LocalPipeClient::open(const std::string & endpointName) {
    std::string pipeName = endpointName;
#ifdef BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE
    if (pipeName[0] != '\\')
        pipeName = "\\\\.\\pipe\\" + pipeName;
    HANDLE h = CreateFileA(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                           OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    return_if(h == INVALID_HANDLE_VALUE, "Error opening pipe", false);
    stream.assign(h);
#else
    boost::system::error_code error;
    if (pipeName[0] != '/')
        pipeName = "/var/run/" + pipeName;
    boost::asio::local::stream_protocol::endpoint ep(pipeName);
    return_if(stream.connect(ep, error), "Error opening pipe: " << error.message(), false);
#endif
    readNextMessage();
    return true;
}
