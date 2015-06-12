/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <boost/test/unit_test.hpp>
#include <boost/asio.hpp>
#include "FlexVDIProto.h"
#include "MessageBuffer.hpp"
#include "LocalPipe.hpp"
#include "util.hpp"
#include "credentials-manager/CredentialsThread.hpp"
#include "TimeoutKiller.hpp"
using namespace std;
namespace ph = std::placeholders;
namespace asio = boost::asio;

namespace flexvm {

#define FIXTURE CredentialsThread_fixture
struct FIXTURE : public CredentialsThread::Listener {
    asio::io_service io;
    CredentialsThread thread;
    LocalPipe pipe;
    Connection::Ptr conn;
    MessageBuffer credentials;
    string recvUser, recvPass, recvDom;
    bool messageHandled, credentialsSent, credentialsReceived;
    TimeoutKiller timeout;

#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS)
    static constexpr const char * pipeName = "/tmp/flexvdi_pipe";
#else
    static constexpr const char * pipeName = "\\\\.\\pipe\\flexvdi_pipe";
#endif

    FIXTURE() : thread(*this), pipe(io, bind(&FIXTURE::handle, this, ph::_1, ph::_2)),
                messageHandled(false), credentialsSent(false), credentialsReceived(false),
                timeout(io, 1000) {
        pipe.setEndpoint(pipeName);
        thread.setEndpoint(pipeName);
        pipe.open();
        credentials = sampleCredentials("user", "pass", "domi");
    }

    ~FIXTURE() {
        ::unlink(pipeName);
    }

    void handle(const Connection::Ptr & src, const MessageBuffer & msg) {
        BOOST_CHECK_EQUAL(msg.header().type, FLEXVDI_ASKCREDENTIALS);
        BOOST_CHECK_EQUAL(msg.header().size, 0);
        conn = src;
        messageHandled = true;
        conn->send(credentials, [this] { credentialsSent = true; });
    }

    virtual void credentialsChanged(const char * u, const char * p, const char * d) {
        credentialsReceived = true;
        recvUser = u;
        recvPass = p;
        recvDom = d;
        io.stop();
    }

    static MessageBuffer sampleCredentials(const char * user, const char * pass, const char * dom) {
        uint32_t userlen = strlen(user), passlen = strlen(pass), domlen = strlen(dom);
        uint32_t bufSize = sizeof(FlexVDICredentialsMsg) + userlen + passlen + domlen + 3;
        MessageBuffer result(FLEXVDI_CREDENTIALS, bufSize);
        auto msg = result.getMessagePtr<FlexVDICredentialsMsg>();
        msg->userLength = userlen;
        msg->passLength = passlen;
        msg->domainLength = domlen;
        std::copy_n(user, userlen + 1, msg->strings);
        std::copy_n(pass, passlen + 1, msg->strings + userlen + 1);
        std::copy_n(dom, domlen + 1, msg->strings + userlen + passlen + 2);
        return result;
    }
};


BOOST_FIXTURE_TEST_CASE(CredentialsThread_testRead, FIXTURE) {
    MessageBuffer credentials = sampleCredentials("user", "pass", "domi");
    thread.requestCredentials();
    io.run();
    io.reset();
    BOOST_CHECK(messageHandled);
    BOOST_CHECK(credentialsSent);
    BOOST_CHECK(credentialsReceived);
    BOOST_CHECK_EQUAL(recvUser, string("user"));
    BOOST_CHECK_EQUAL(recvPass, string("pass"));
    BOOST_CHECK_EQUAL(recvDom, string("domi"));
}

} // namespace flexvm
