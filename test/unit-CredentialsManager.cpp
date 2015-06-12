
/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <boost/test/unit_test.hpp>
#include "credentials-manager/CredentialsManager.hpp"
#include "Connection.hpp"
using namespace std;

namespace flexvm {

class MockCredConnection : public Connection {
public:
    MockCredConnection() : Connection([] (const Ptr &, const MessageBuffer &) {}) {}
    virtual bool isOpen() const { return true; }
    virtual void close() {}
    virtual void asyncRead(const boost::asio::mutable_buffers_1 & buffers,
                           AsyncHandler handler) {}
    virtual void asyncWrite(const MessageBuffer & buffer, AsyncHandler handler) {
        BOOST_REQUIRE_EQUAL(buffer.header().type, FLEXVDI_CREDENTIALS);
        sentCreds = buffer;
        handler(boost::system::error_code(), buffer.size());
    }

    MessageBuffer sentCreds;
};

#define FIXTURE CredentialsManager_fixture
struct FIXTURE {
    CredentialsManager manager;
    shared_ptr<MockCredConnection> conn;

    FIXTURE() : conn(new MockCredConnection) {}

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


BOOST_FIXTURE_TEST_CASE(CredentialsManager_testAskFirst, FIXTURE) {
    MessageBuffer credentialsBuffer = sampleCredentials("user", "pass", "domi");
    auto credentials = credentialsBuffer.getMessagePtr<FlexVDICredentialsMsg>();
    auto askCredentials = MessageBuffer(FLEXVDI_ASKCREDENTIALS, 0)
                              .getMessagePtr<FlexVDIAskCredentialsMsg>();
    manager.handle(conn, askCredentials);
    // No credentials yet
    BOOST_CHECK(!conn->sentCreds.shareData().get());
    manager.handle(conn, credentials);
    // Credentials have been erased
    string user, pass, dom;
    user = getCredentialsUser(credentials.get());
    pass = getCredentialsPassword(credentials.get());
    dom = getCredentialsDomain(credentials.get());
    BOOST_CHECK_EQUAL(user, string());
    BOOST_CHECK_EQUAL(pass, string());
    BOOST_CHECK_EQUAL(dom, string());
    // Credentials set
    BOOST_REQUIRE(conn->sentCreds.shareData().get());
    credentials = conn->sentCreds.getMessagePtr<FlexVDICredentialsMsg>();
    user = getCredentialsUser(credentials.get());
    pass = getCredentialsPassword(credentials.get());
    dom = getCredentialsDomain(credentials.get());
    BOOST_CHECK_EQUAL(user, string("user"));
    BOOST_CHECK_EQUAL(pass, string("pass"));
    BOOST_CHECK_EQUAL(dom, string("domi"));
}


BOOST_FIXTURE_TEST_CASE(CredentialsManager_testAskLater, FIXTURE) {
    MessageBuffer credentialsBuffer = sampleCredentials("user", "pass", "domi");
    auto credentials = credentialsBuffer.getMessagePtr<FlexVDICredentialsMsg>();
    auto askCredentials = MessageBuffer(FLEXVDI_ASKCREDENTIALS, 0)
                              .getMessagePtr<FlexVDIAskCredentialsMsg>();
    manager.handle(conn, credentials);
    // No credentials yet
    BOOST_CHECK(!conn->sentCreds.shareData().get());
    manager.handle(conn, askCredentials);
    // Credentials have been erased
    string user, pass, dom;
    user = getCredentialsUser(credentials.get());
    pass = getCredentialsPassword(credentials.get());
    dom = getCredentialsDomain(credentials.get());
    BOOST_CHECK_EQUAL(user, string());
    BOOST_CHECK_EQUAL(pass, string());
    BOOST_CHECK_EQUAL(dom, string());
    // Credentials set
    BOOST_REQUIRE(conn->sentCreds.shareData().get());
    credentials = conn->sentCreds.getMessagePtr<FlexVDICredentialsMsg>();
    user = getCredentialsUser(credentials.get());
    pass = getCredentialsPassword(credentials.get());
    dom = getCredentialsDomain(credentials.get());
    BOOST_CHECK_EQUAL(user, string("user"));
    BOOST_CHECK_EQUAL(pass, string("pass"));
    BOOST_CHECK_EQUAL(dom, string("domi"));
}

} // namespace flexvm
