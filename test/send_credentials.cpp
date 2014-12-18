/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include <memory>
#include <algorithm>
#include <boost/asio.hpp>
#define FLEXVDI_PROTO_IMPL
#include "../FlexVDIProto.h"
#include "../SharedConstBuffer.hpp"

#ifdef BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE
#include <windows.h>
#endif

using namespace std;
namespace asio = boost::asio;

int main(int argc, char * argv[]) {
    if (argc != 2) {
        clog << "Use: send_credentials pipe" << endl;
        return 1;
    }
    auto mHeader = new FlexVDIMessageHeader;
    mHeader->type = FLEXVDI_CREDENTIALS;
    mHeader->size = sizeof(FlexVDICredentialsMsg) + 15;
    auto msg = new FlexVDICredentialsMsg;
    msg->domainLength = msg->passLength = msg->userLength = 4;
    auto msgBuffer = new uint8_t[15];
    std::copy_n("user", 5, msgBuffer);
    std::copy_n("pass", 5, msgBuffer + 5);
    std::copy_n("domi", 5, msgBuffer + 10);
    asio::io_service io;
#ifdef BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR
    asio::posix::stream_descriptor stream(io, ::open(argv[1], O_RDWR));
#else
    HANDLE h = ::CreateFile(argv[1], GENERIC_READ | GENERIC_WRITE, 0, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    asio::windows::stream_handle stream(io, h);
#endif
    asio::write<>(stream, flexvm::SharedConstBuffer(mHeader)(msg)(msgBuffer, 15));
    return 0;
}
