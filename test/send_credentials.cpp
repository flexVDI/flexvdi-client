/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include <memory>
#include <algorithm>
#include <cstring>
#include <boost/asio.hpp>
#include "FlexVDIProto.h"
#include "MessageBuffer.hpp"
#include "util.hpp"
#include "credentials-manager/CredentialsThread.hpp"

#ifdef BOOST_ASIO_HAS_WINDOWS_STREAM_HANDLE
#include <windows.h>
#endif

using namespace std;
using namespace flexvm;
namespace asio = boost::asio;

static constexpr uint32_t CRED_SIZE = sizeof(FlexVDICredentialsMsg);


MessageBuffer sampleCredentials(const char * user, const char * pass, const char * dom) {
    uint32_t userlen = strlen(user), passlen = strlen(pass), domlen = strlen(dom);
    uint32_t bufSize = userlen + passlen + domlen + 3;
    MessageBuffer result(FLEXVDI_CREDENTIALS, CRED_SIZE + bufSize);
    auto msg = result.getMessagePtr<FlexVDICredentialsMsg>();
    msg->userLength = userlen;
    msg->passLength = passlen;
    msg->domainLength = domlen;
    std::copy_n(user, userlen + 1, msg->strings);
    std::copy_n(pass, passlen + 1, msg->strings + userlen + 1);
    std::copy_n(dom, domlen + 1, msg->strings + userlen + passlen + 2);
    return result;
}


boost::barrier barrier(2);


struct CredentialsListener : public CredentialsThread::Listener {
    virtual void credentialsChanged(const char * u, const char * p, const char * d) {
        clog << "Received credentials " << u << ", " << p << ", " << d << endl;
        barrier.wait();
    }
} listener ;


int main(int argc, char * argv[]) {
    if (argc != 2) {
        clog << "Use: send_credentials socket|pipe" << endl;
        return 1;
    }
    asio::io_service io;
#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS)
    asio::local::stream_protocol::endpoint ep(argv[1]);
    asio::local::stream_protocol::socket pipe(io);
    pipe.connect(ep);
#else
    HANDLE h;
    h = ::CreateFileA(argv[1], GENERIC_READ | GENERIC_WRITE, 0, NULL,
                      OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    asio::windows::stream_handle pipe(io, h);
#endif

    CredentialsThread thread(listener);

    clog << "Sending credentials, ";
    clog << asio::write<>(pipe, sampleCredentials("user", "pass", "domi")) << " bytes written" << endl;
    thread.requestCredentials();
    barrier.wait();
    clog << "Sending credentials, ";
    clog << asio::write<>(pipe, sampleCredentials("user", "pass", "domi")) << " bytes written" << endl;
    thread.requestCredentials();
    barrier.wait();
    thread.requestCredentials();
    for (int i = 0; i < 100000000; ++i);
    thread.stop();
    clog << "Sending credentials, ";
    clog << asio::write<>(pipe, sampleCredentials("user", "pass", "domi")) << " bytes written" << endl;
    thread.requestCredentials();
    barrier.wait();

    return 0;
}
