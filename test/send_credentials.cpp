/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <iostream>
#include <memory>
#include <algorithm>
#include <cstring>
#include <boost/asio.hpp>
#include "../FlexVDIProto.h"
#include "../SharedConstBuffer.hpp"
#include "../util.hpp"
#include "../cred-connectors/CredentialsThread.hpp"

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
    uint8_t * msgBuffer = new uint8_t[bufSize];
    std::copy_n(user, userlen + 1, msgBuffer);
    std::copy_n(pass, passlen + 1, msgBuffer + userlen + 1);
    std::copy_n(dom, domlen + 1, msgBuffer + userlen + passlen + 2);
    return SharedConstBuffer(mHeader)(msg)
        (shared_ptr<uint8_t>(msgBuffer, std::default_delete<uint8_t[]>()), bufSize);
}


boost::barrier barrier(2);


struct CredentialsListener : public CredentialsThread::Listener {
    virtual void credentialsChanged(const char * u, const char * p, const char * d) {
        clog << "Received credentials " << u << ", " << p << ", " << d << endl;
        barrier.wait();
    }
} listener ;


int main(int argc, char * argv[]) {
    if (argc != 3) {
        clog << "Use: send_credentials port pipe" << endl;
        return 1;
    }
    asio::io_service io;
#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS)
    asio::posix::stream_descriptor port(io, ::open(argv[1], O_RDWR));
    asio::local::stream_protocol::endpoint ep(argv[2]);
    asio::local::stream_protocol::socket pipe(io);
    pipe.connect(ep);
#else
    HANDLE h;
    h = ::CreateFileA(argv[2], GENERIC_READ | GENERIC_WRITE, 0, NULL,
                      OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    DWORD dwMode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(h, &dwMode, NULL, NULL);
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
