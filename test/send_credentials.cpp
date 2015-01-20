/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include <memory>
#include <algorithm>
#include <cstring>
#include <boost/asio.hpp>
#define FLEXVDI_PROTO_IMPL
#include "../FlexVDIProto.h"
#include "../SharedConstBuffer.hpp"

#ifdef BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE
#include <windows.h>
#endif

using namespace std;
using namespace flexvm;
namespace asio = boost::asio;

static constexpr uint32_t CRED_SIZE = sizeof(FlexVDICredentialsMsg);


SharedConstBuffer sampleCredentials(const char * user, const char * pass, const char * dom) {
    uint32_t userlen = strlen(user), passlen = strlen(pass), domlen = strlen(dom);
    uint32_t bufSize = userlen + passlen + domlen + 3;
    auto mHeader = new FlexVDIMessageHeader{FLEXVDI_CREDENTIALS, CRED_SIZE + bufSize};
    auto msg = new FlexVDICredentialsMsg{userlen, passlen, domlen};
    auto msgBuffer = new uint8_t[bufSize];
    std::copy_n(user, userlen + 1, msgBuffer);
    std::copy_n(pass, passlen + 1, msgBuffer + userlen + 1);
    std::copy_n(dom, domlen + 1, msgBuffer + userlen + passlen + 2);
    return SharedConstBuffer(mHeader)(msg)(msgBuffer, bufSize);
}


SharedConstBuffer askCredentials() {
    auto mHeader = new FlexVDIMessageHeader{FLEXVDI_ASKCREDENTIALS, 0};
    return SharedConstBuffer(mHeader);
}


int main(int argc, char * argv[]) {
    if (argc != 3) {
        clog << "Use: send_credentials port pipe" << endl;
        return 1;
    }
    asio::io_service io;
#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR) && defined(BOOST_ASIO_HAS_LOCAL_SOCKETS)
    asio::posix::stream_descriptor port(io, ::open(argv[1], O_RDWR));
    asio::local::stream_protocol::endpoint ep(argv[2]);
    asio::local::stream_protocol::socket pipe(io);
    pipe.connect(ep);
#else
    HANDLE h = ::CreateFileA(argv[1], GENERIC_READ | GENERIC_WRITE, 0, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    asio::windows::stream_handle stream(io, h);
#endif

    FlexVDIMessageHeader header;
    std::unique_ptr<uint8_t[]> buffer;
    FlexVDICredentialsMsg * msg;

    // Get empty credentials
    clog << "Requesting credentials, ";
    clog << asio::write<>(pipe, askCredentials()) << " bytes written,";
    clog << asio::read<>(pipe, asio::buffer(&header, sizeof(header))) << " bytes read" << endl;
    clog << "Read header of type " << header.type << " and size " << header.size << endl;
    buffer.reset(new uint8_t[header.size]);
    clog << asio::read<>(pipe, asio::buffer(buffer.get(), header.size)) << " bytes read" << endl;
    msg = (FlexVDICredentialsMsg *)buffer.get();
    clog << "Credentials: " << getCredentialsUser(msg) << ", " << getCredentialsPassword(msg) <<
    ", " << getCredentialsDomain(msg) << endl;

    // Get true credentials
    clog << "Sending credentials, ";
    clog << asio::write<>(port, sampleCredentials("user", "pass", "domi")) << " bytes written" << endl;
    sleep(1);
    clog << "Requesting credentials, ";
    clog << asio::write<>(pipe, askCredentials()) << " bytes written,";
    clog << asio::read<>(pipe, asio::buffer(&header, sizeof(header))) << " bytes read" << endl;
    clog << "Read header of type " << header.type << " and size " << header.size << endl;
    buffer.reset(new uint8_t[header.size]);
    clog << asio::read<>(pipe, asio::buffer(buffer.get(), header.size)) << " bytes read" << endl;
    msg = (FlexVDICredentialsMsg *)buffer.get();
    clog << "Credentials: " << getCredentialsUser(msg) << ", " << getCredentialsPassword(msg) <<
    ", " << getCredentialsDomain(msg) << endl;
    
    return 0;
}
